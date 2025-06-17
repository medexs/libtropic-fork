import logging
from enum import Enum
from pathlib import Path
import serial
import time

from .lt_platform_factory import lt_platform_factory
from .lt_platform import lt_platform
from .lt_environment_tools import lt_environment_tools
from .lt_openocd_launcher import lt_openocd_launcher

logger = logging.getLogger(__name__)

class lt_test_runner:
    class lt_test_result(Enum):
        TEST_FAILED = -1
        TEST_PASSED = 0

    class OpenOCDConnectionErrorException(BaseException):
        pass

    def __init__(self, working_dir: Path, platform_id: str, mapping_config_path: Path, adapter_config_path: Path):
        self.working_dir = working_dir
        logger.info("Preparing environment...")
        self.working_dir.mkdir(exist_ok = True, parents = True)


        self.platform = lt_platform_factory.create_from_str_id(platform_id)
        if self.platform is None:
            logger.error(f"Platform '{self.platform}' not found!")
            raise ValueError
        
        adapter_id = lt_environment_tools.get_adapter_id_from_mapping(platform_id, mapping_config_path)
        if adapter_id is None:
            logger.error(f"Selected platform has not its adapter ID assigned yet in the config file. Assign the ID in {mapping_config_path} (or select correct config file) and try again.")
            raise ValueError
    
        self.serial_port  = lt_environment_tools.get_serial_device_from_vidpid(adapter_id.vid, adapter_id.pid, 1) # TS11 adapter uses second interface for serial port.
        if self.serial_port is None:
            logger.error("Serial adapter not found. Check if adapter is correctly connected to the computer and correct interface is selected.")
            raise ValueError
        
        openocd_launch_params = ["-f", adapter_config_path] + ["-c", f"ftdi vid_pid {adapter_id.vid:#x} {adapter_id.pid:#x}"] + self.platform.get_openocd_launch_params() 

        try:
            self.openocd_launcher = lt_openocd_launcher(openocd_launch_params)
        except FileNotFoundError:
            logger.error("Couldn't find OpenOCD!")
            raise

        time.sleep(2)
        if not self.openocd_launcher.is_running():
            logger.error("Couldn't launch OpenOCD. Hint: run with higher verbosity (-v) to check OpenOCD output.")
            return

    def __del__(self):
        pass

    async def run(self, elf_path: Path) -> lt_test_result:
        
        if not await self.platform.openocd_connect():
            logger.error("Couldn't connect to OpenOCD!")
            return self.lt_test_result.TEST_FAILED

        await self.platform.initialize()

        if not await self.platform.is_spi_bus_free():
            logger.error("SPI bus is already occupied! Check if all other platform boards disabled SPI access!")
            self.platform.openocd_disconnect()
            return self.lt_test_result.TEST_FAILED

        await self.platform.set_spi_en(True)
        await self.platform.set_platform_power(True)

        await self.platform.blink_disco_led(lt_platform.lt_led_color.WHITE)
        await self.platform.set_disco_led(lt_platform.lt_led_color.WHITE)

        try:
            await self.platform.load_elf(elf_path)
        except TimeoutError:
            logger.error("Communication with OpenOCD timed out while loading firmware!")
            await self.platform.set_platform_power(False)
            self.platform.openocd_disconnect()
            return self.lt_test_result.TEST_FAILED

        err_count = 0
        warn_count = 0
        assert_fail_count = 0
        try:
            with serial.Serial(self.serial_port, baudrate = 115200, timeout=10, inter_byte_timeout=10, exclusive=True) as s:
                await self.platform.reset()
            
                while True:
                    if not s.is_open:
                        logger.error("Serial port closed! Check if it is not used by other application (screen, GTKTerm...).")
                        break

                    line = s.read_until(expected=b"\r\n").decode('ascii', errors="backslashreplace")
                    logger.debug(f"Received from serial: {line}")
                    line = line.split(";")
                    if (len(line) < 3):
                        logger.error("Line malformed!")
                        continue
                    
                    code_line_number = line[0].strip()
                    msg_type = line[1].strip()
                    msg_content = line[2].strip()

                    if (msg_type == "INFO"):
                        logger.info(f"[PLATFORM][{code_line_number}] {msg_content}")
                    elif (msg_type == "WARNING"):
                        logger.warning(f"[PLATFORM][{code_line_number}] {msg_content}")
                        warn_count += 1
                    elif (msg_type == "ERROR"):
                        logger.error(f"[PLATFORM][{code_line_number}] {msg_content}")
                        err_count += 1
                    elif (msg_type == "SYSTEM"):
                        if msg_content == "ASSERT_OK":
                            logger.debug(f"Assertion at [{code_line_number}] passed!")
                        elif msg_content == "ASSERT_FAIL":
                            if (len(line) < 5):
                                logger.error("Line malformed!")
                                continue
                            msg_expected = line[4].strip()
                            msg_got = line[3].strip()
                            logger.error(f"Assertion at [{code_line_number}] failed! Expected '{msg_expected}', got '{msg_got}'.")
                            assert_fail_count += 1
                        elif msg_content == "TEST_FINISH":
                            logger.info("Received TEST_FINISH, wrapping up.")
                            break
        except TimeoutError:
            logger.error("Serial timeout! Did not receive any message in time. Check if the test output is correct and if TEST_FINISH is issued at the end of the test.")

        logger.info("------ Stats ------")
        logger.info(f"Errors: {err_count}")
        logger.info(f"Warnings: {warn_count}")
        logger.info(f"Failed assertions: {assert_fail_count}")

        if err_count > 0 or warn_count > 0 or assert_fail_count > 0:
            logger.error("There were errors or warnings, test unsuccessful!")
            await self.platform.set_disco_led(lt_platform.lt_led_color.RED)
            await self.platform.set_platform_power(False)
            self.platform.openocd_disconnect()
            return self.lt_test_result.TEST_FAILED

        await self.platform.set_disco_led(lt_platform.lt_led_color.GREEN)
        await self.platform.set_platform_power(False)
        self.platform.openocd_disconnect()
        return self.lt_test_result.TEST_PASSED