"""ESPHome component for Wi-Fi HaLow (IEEE 802.11ah) via Morse Micro MM6108."""

import logging
import os
import esphome.codegen as cg

_LOGGER = logging.getLogger(__name__)
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_PASSWORD,
    CONF_SSID,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_DECIBEL_MILLIWATT,
)
from esphome.core import coroutine_with_priority, CORE
from esphome.components.esp32 import add_idf_sdkconfig_option, add_idf_component

DEPENDENCIES = ["esp32"]
AUTO_LOAD = ["network", "sensor", "text_sensor", "button"]
CONFLICTS_WITH = ["wifi"]

CONF_COUNTRY_CODE = "country_code"
CONF_SECURITY = "security"
CONF_BCF_FILE = "bcf_file"
CONF_MM_IOT_SDK_PATH = "mm_iot_sdk_path"
CONF_MANUAL_IP = "manual_ip"
CONF_STATIC_IP = "static_ip"
CONF_GATEWAY = "gateway"
CONF_SUBNET = "subnet"
CONF_DNS1 = "dns1"
CONF_DNS2 = "dns2"
CONF_CLK_PIN = "clk_pin"
CONF_MOSI_PIN = "mosi_pin"
CONF_MISO_PIN = "miso_pin"
CONF_CS_PIN = "cs_pin"
CONF_IRQ_PIN = "irq_pin"
CONF_RESET_PIN = "reset_pin"
CONF_WAKE_PIN = "wake_pin"
CONF_BUSY_PIN = "busy_pin"

# Sensor config keys
CONF_RSSI_SENSOR = "rssi"
CONF_TX_PPS = "tx_pps"
CONF_RX_PPS = "rx_pps"
CONF_TX_KBPS = "tx_kbps"
CONF_RX_KBPS = "rx_kbps"
CONF_MCS = "mcs"
CONF_BANDWIDTH = "bandwidth"
CONF_TX_SUCCESS_RATE = "tx_success_rate"
CONF_LINK_QUALITY = "link_quality"
CONF_NOISE_FLOOR = "noise_floor"
CONF_TX_FRAMES_DROPPED = "tx_frames_dropped"
CONF_RX_FRAMES_DROPPED = "rx_frames_dropped"
CONF_CCMP_FAILURES = "ccmp_failures"
CONF_HW_RESTARTS = "hw_restarts"
CONF_SCAN_RESULTS = "scan_results"
CONF_SCAN_BUTTON = "scan_channel"
CONF_SUB_BANDS = "sub_bands"
CONF_IP_ADDRESS_SENSOR = "ip_address"
CONF_GATEWAY_SENSOR = "gateway_address"
CONF_SUBNET_SENSOR = "subnet_mask"
CONF_SSID_SENSOR = "connected_ssid"
CONF_BSSID_SENSOR = "bssid"
CONF_MAC_ADDRESS_SENSOR = "mac_address"
CONF_FW_VERSION_SENSOR = "firmware_version"
CONF_TYPE = "type"
CONF_MODE = "mode"

CHIP_TYPES = {"MM6108": "MM6108", "MM8108": "MM8108"}
MODE_TYPES = {"STATION": "STATION", "AP": "AP"}

halow_ns = cg.esphome_ns.namespace("halow")
HalowComponent = halow_ns.class_("HalowComponent", cg.Component)
HalowScanButton = halow_ns.class_(
    "HalowScanButton", cg.esphome_ns.namespace("button").class_("Button")
)

MANUAL_IP_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_STATIC_IP): cv.ipv4address,
        cv.Required(CONF_GATEWAY): cv.ipv4address,
        cv.Required(CONF_SUBNET): cv.ipv4address,
        cv.Optional(CONF_DNS1, default="0.0.0.0"): cv.ipv4address,
        cv.Optional(CONF_DNS2, default="0.0.0.0"): cv.ipv4address,
    }
)

SECURITY_TYPES = {"SAE": "SAE", "OWE": "OWE", "OPEN": "OPEN"}


def _sensor_schema(**kwargs):
    """Import sensor schema lazily to avoid circular deps."""
    import esphome.components.sensor as sensor_ns

    return sensor_ns.sensor_schema(**kwargs)


def _text_sensor_schema(**kwargs):
    """Import text_sensor schema lazily to avoid circular deps."""
    import esphome.components.text_sensor as ts_ns

    return ts_ns.text_sensor_schema(**kwargs)


def _button_schema(class_, **kwargs):
    """Import button schema lazily to avoid circular deps."""
    import esphome.components.button as button_ns

    return button_ns.button_schema(class_, **kwargs)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(HalowComponent),
            cv.Optional(CONF_TYPE, default="MM6108"): cv.enum(
                CHIP_TYPES, upper=True
            ),
            cv.Optional(CONF_MODE, default="STATION"): cv.enum(
                MODE_TYPES, upper=True
            ),
            cv.Required(CONF_SSID): cv.string,
            cv.Required(CONF_PASSWORD): cv.string,
            cv.Optional(CONF_COUNTRY_CODE, default="US"): cv.string_strict,
            cv.Optional(CONF_SECURITY, default="SAE"): cv.enum(
                SECURITY_TYPES, upper=True
            ),
            cv.Optional(
                CONF_BCF_FILE, default="bcf_mf16858.mbin"
            ): cv.string_strict,
            cv.Optional(
                CONF_MM_IOT_SDK_PATH, default="~/.esphome/mm-iot-esp32"
            ): cv.string,
            cv.Optional(CONF_SUB_BANDS, default=False): cv.boolean,
            cv.Optional(CONF_MANUAL_IP): MANUAL_IP_SCHEMA,
            # SPI pins — defaults match XIAO HaLow Hat
            cv.Optional(CONF_CLK_PIN, default=7): cv.int_,
            cv.Optional(CONF_MOSI_PIN, default=9): cv.int_,
            cv.Optional(CONF_MISO_PIN, default=8): cv.int_,
            cv.Optional(CONF_CS_PIN, default=4): cv.int_,
            cv.Optional(CONF_IRQ_PIN, default=3): cv.int_,
            cv.Optional(CONF_RESET_PIN, default=1): cv.int_,
            cv.Optional(CONF_WAKE_PIN, default=2): cv.int_,
            cv.Optional(CONF_BUSY_PIN, default=5): cv.int_,
            # Numeric sensors
            cv.Optional(CONF_RSSI_SENSOR): _sensor_schema(
                unit_of_measurement=UNIT_DECIBEL_MILLIWATT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_TX_PPS): _sensor_schema(
                unit_of_measurement="pps",
                accuracy_decimals=1,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                icon="mdi:upload-network",
            ),
            cv.Optional(CONF_RX_PPS): _sensor_schema(
                unit_of_measurement="pps",
                accuracy_decimals=1,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                icon="mdi:download-network",
            ),
            cv.Optional(CONF_TX_KBPS): _sensor_schema(
                unit_of_measurement="kbps",
                accuracy_decimals=1,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                icon="mdi:upload-network",
            ),
            cv.Optional(CONF_RX_KBPS): _sensor_schema(
                unit_of_measurement="kbps",
                accuracy_decimals=1,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                icon="mdi:download-network",
            ),
            cv.Optional(CONF_MCS): _sensor_schema(
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                icon="mdi:signal-variant",
            ),
            cv.Optional(CONF_BANDWIDTH): _sensor_schema(
                unit_of_measurement="MHz",
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                icon="mdi:signal-variant",
            ),
            cv.Optional(CONF_TX_SUCCESS_RATE): _sensor_schema(
                unit_of_measurement="%",
                accuracy_decimals=1,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                icon="mdi:check-network",
            ),
            # Text sensors
            cv.Optional(CONF_IP_ADDRESS_SENSOR): _text_sensor_schema(
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                icon="mdi:ip-network",
            ),
            cv.Optional(CONF_GATEWAY_SENSOR): _text_sensor_schema(
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                icon="mdi:router-wireless",
            ),
            cv.Optional(CONF_SUBNET_SENSOR): _text_sensor_schema(
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_SSID_SENSOR): _text_sensor_schema(
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                icon="mdi:wifi",
            ),
            cv.Optional(CONF_BSSID_SENSOR): _text_sensor_schema(
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_MAC_ADDRESS_SENSOR): _text_sensor_schema(
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_FW_VERSION_SENSOR): _text_sensor_schema(
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                icon="mdi:chip",
            ),
            cv.Optional(CONF_LINK_QUALITY): _text_sensor_schema(
                icon="mdi:wifi-strength-outline",
            ),
            cv.Optional(CONF_SCAN_RESULTS): _text_sensor_schema(
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                icon="mdi:radar",
            ),
            # UMAC diagnostic sensors
            cv.Optional(CONF_NOISE_FLOOR): _sensor_schema(
                unit_of_measurement="dBm",
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                icon="mdi:sine-wave",
            ),
            cv.Optional(CONF_TX_FRAMES_DROPPED): _sensor_schema(
                accuracy_decimals=0,
                state_class=STATE_CLASS_TOTAL_INCREASING,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                icon="mdi:alert-circle-outline",
            ),
            cv.Optional(CONF_RX_FRAMES_DROPPED): _sensor_schema(
                accuracy_decimals=0,
                state_class=STATE_CLASS_TOTAL_INCREASING,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                icon="mdi:alert-circle-outline",
            ),
            cv.Optional(CONF_CCMP_FAILURES): _sensor_schema(
                accuracy_decimals=0,
                state_class=STATE_CLASS_TOTAL_INCREASING,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                icon="mdi:shield-alert-outline",
            ),
            cv.Optional(CONF_HW_RESTARTS): _sensor_schema(
                accuracy_decimals=0,
                state_class=STATE_CLASS_TOTAL_INCREASING,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                icon="mdi:restart-alert",
            ),
            # Channel scan button
            cv.Optional(CONF_SCAN_BUTTON): _button_schema(
                HalowScanButton,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                icon="mdi:radar",
            ),
        }
    ),
    cv.only_on_esp32,
)


@coroutine_with_priority(60.0)  # Same priority as WiFi/Ethernet
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Core configuration
    cg.add(var.set_ssid(config[CONF_SSID]))
    cg.add(var.set_password(config[CONF_PASSWORD]))
    cg.add(var.set_country_code(config[CONF_COUNTRY_CODE]))
    cg.add(var.set_security_type(config[CONF_SECURITY]))
    cg.add(var.set_sub_bands_enabled(config[CONF_SUB_BANDS]))

    # Pin configuration
    cg.add(var.set_spi_clk_pin(config[CONF_CLK_PIN]))
    cg.add(var.set_spi_mosi_pin(config[CONF_MOSI_PIN]))
    cg.add(var.set_spi_miso_pin(config[CONF_MISO_PIN]))
    cg.add(var.set_spi_cs_pin(config[CONF_CS_PIN]))
    cg.add(var.set_spi_irq_pin(config[CONF_IRQ_PIN]))
    cg.add(var.set_reset_pin(config[CONF_RESET_PIN]))
    cg.add(var.set_wake_pin(config[CONF_WAKE_PIN]))
    cg.add(var.set_busy_pin(config[CONF_BUSY_PIN]))

    # Static IP
    if manual_ip := config.get(CONF_MANUAL_IP):
        cg.add(
            var.set_manual_ip(
                str(manual_ip[CONF_STATIC_IP]),
                str(manual_ip[CONF_GATEWAY]),
                str(manual_ip[CONF_SUBNET]),
                str(manual_ip.get(CONF_DNS1, "0.0.0.0")),
                str(manual_ip.get(CONF_DNS2, "0.0.0.0")),
            )
        )

    # Sensors
    import esphome.components.sensor as sensor_ns
    import esphome.components.text_sensor as ts_ns

    for conf_key, setter in [
        (CONF_RSSI_SENSOR, var.set_rssi_sensor),
        (CONF_TX_PPS, var.set_tx_pps_sensor),
        (CONF_RX_PPS, var.set_rx_pps_sensor),
        (CONF_TX_KBPS, var.set_tx_kbps_sensor),
        (CONF_RX_KBPS, var.set_rx_kbps_sensor),
        (CONF_MCS, var.set_mcs_sensor),
        (CONF_BANDWIDTH, var.set_bandwidth_sensor),
        (CONF_TX_SUCCESS_RATE, var.set_tx_success_rate_sensor),
        (CONF_NOISE_FLOOR, var.set_noise_floor_sensor),
        (CONF_TX_FRAMES_DROPPED, var.set_tx_frames_dropped_sensor),
        (CONF_RX_FRAMES_DROPPED, var.set_rx_frames_dropped_sensor),
        (CONF_CCMP_FAILURES, var.set_ccmp_failures_sensor),
        (CONF_HW_RESTARTS, var.set_hw_restarts_sensor),
    ]:
        if conf_key in config:
            sens = await sensor_ns.new_sensor(config[conf_key])
            cg.add(setter(sens))

    for conf_key, setter in [
        (CONF_IP_ADDRESS_SENSOR, var.set_ip_address_sensor),
        (CONF_GATEWAY_SENSOR, var.set_gateway_sensor),
        (CONF_SUBNET_SENSOR, var.set_subnet_sensor),
        (CONF_SSID_SENSOR, var.set_ssid_sensor),
        (CONF_BSSID_SENSOR, var.set_bssid_sensor),
        (CONF_MAC_ADDRESS_SENSOR, var.set_mac_address_sensor),
        (CONF_FW_VERSION_SENSOR, var.set_fw_version_sensor),
        (CONF_LINK_QUALITY, var.set_link_quality_sensor),
        (CONF_SCAN_RESULTS, var.set_scan_results_sensor),
    ]:
        if conf_key in config:
            sens = await ts_ns.new_text_sensor(config[conf_key])
            cg.add(setter(sens))

    # Scan button
    if CONF_SCAN_BUTTON in config:
        import esphome.components.button as button_ns

        btn = await button_ns.new_button(config[CONF_SCAN_BUTTON])
        cg.add(btn.set_parent(var))

    cg.add_define("USE_HALOW")

    # --- MM-IoT-SDK Integration ---

    # Resolve and auto-download SDK if needed
    MM_IOT_SDK_REPO = "https://github.com/MorseMicro/mm-iot-esp32.git"

    sdk_path = os.path.expanduser(config[CONF_MM_IOT_SDK_PATH])
    sdk_path = os.path.abspath(sdk_path)
    framework_path = os.path.join(sdk_path, "framework")

    if not os.path.isdir(framework_path):
        import subprocess

        _LOGGER.info("Downloading MM-IoT-SDK to %s ...", sdk_path)
        result = subprocess.run(
            ["git", "clone", "--depth", "1", MM_IOT_SDK_REPO, sdk_path],
            capture_output=True,
            text=True,
        )
        if result.returncode != 0:
            raise cv.Invalid(
                f"Failed to download MM-IoT-SDK: {result.stderr.strip()}\n"
                f"Manually clone: git clone {MM_IOT_SDK_REPO} {sdk_path}"
            )
        _LOGGER.info("MM-IoT-SDK downloaded successfully")

    # Register IDF components
    components = [
        ("morselib", "morselib"),
        ("mm_shims", "mm_shims"),
        ("mmipal", "src/mmipal"),
        ("mmutils", "src/mmutils"),
        ("mmpktmem", "src/mmpktmem"),
    ]
    # mmregdb exists in upstream SDK but not in Seeed fork
    mmregdb_path = os.path.join(framework_path, "src", "mmregdb")
    if os.path.isdir(mmregdb_path):
        components.append(("mmregdb", "src/mmregdb"))
        cg.add_define("USE_HALOW_REGDB")
    for name, subpath in components:
        add_idf_component(name=name, path=os.path.join(framework_path, subpath))

    # Kconfig: Pins
    add_idf_sdkconfig_option("CONFIG_MM_RESET_N", config[CONF_RESET_PIN])
    add_idf_sdkconfig_option("CONFIG_MM_WAKE", config[CONF_WAKE_PIN])
    add_idf_sdkconfig_option("CONFIG_MM_BUSY", config[CONF_BUSY_PIN])
    add_idf_sdkconfig_option("CONFIG_MM_SPI_SCK", config[CONF_CLK_PIN])
    add_idf_sdkconfig_option("CONFIG_MM_SPI_MOSI", config[CONF_MOSI_PIN])
    add_idf_sdkconfig_option("CONFIG_MM_SPI_MISO", config[CONF_MISO_PIN])
    add_idf_sdkconfig_option("CONFIG_MM_SPI_CS", config[CONF_CS_PIN])
    add_idf_sdkconfig_option("CONFIG_MM_SPI_IRQ", config[CONF_IRQ_PIN])

    # Kconfig: Firmware files — differs between upstream and Seeed SDK
    kconfig_path = os.path.join(framework_path, "mm_shims", "Kconfig")
    with open(kconfig_path) as f:
        kconfig_content = f.read()
    if "MM_BCF_FILE" in kconfig_content:
        # Upstream SDK: string-based BCF/FW config
        add_idf_sdkconfig_option("CONFIG_MM_BCF_FILE", config[CONF_BCF_FILE])
        add_idf_sdkconfig_option("CONFIG_MM_FW_FILE", "mm6108.mbin")
    else:
        # Seeed SDK: choice-based BCF config
        bcf = config[CONF_BCF_FILE]
        if "mf16858_us" in bcf:
            add_idf_sdkconfig_option("CONFIG_MM_BCF_MF16858_US", True)
        elif "mf08651_us" in bcf:
            add_idf_sdkconfig_option("CONFIG_MM_BCF_MF08651_US", True)
        elif "mf08551" in bcf:
            add_idf_sdkconfig_option("CONFIG_MM_BCF_MF08551", True)
        elif "mf08251" in bcf:
            add_idf_sdkconfig_option("CONFIG_MM_BCF_MF08251", True)
    # Chip type
    chip_type = config[CONF_TYPE]
    if chip_type == "MM8108":
        add_idf_sdkconfig_option("CONFIG_MMHAL_CHIP_TYPE_MM8108", True)
    else:
        add_idf_sdkconfig_option("CONFIG_MMHAL_CHIP_TYPE_MM6108", True)

    # Kconfig: FreeRTOS
    add_idf_sdkconfig_option("CONFIG_FREERTOS_HZ", 1000)
    add_idf_sdkconfig_option("CONFIG_FREERTOS_TIMER_TASK_PRIORITY", 10)

    # Kconfig: ESP32-S3
    add_idf_sdkconfig_option("CONFIG_ESP32S3_INSTRUCTION_CACHE_32KB", True)
    add_idf_sdkconfig_option("CONFIG_MBEDTLS_NIST_KW_C", True)

    # Kconfig: LWIP settings for mmipal
    add_idf_sdkconfig_option("CONFIG_LWIP_NETIF_STATUS_CALLBACK", True)
    add_idf_sdkconfig_option("CONFIG_LWIP_STATS", True)  # Needed for TX/RX packet counters
    cg.add_build_flag("-DLWIP_NETIF_LINK_CALLBACK=1")

    # Disable mDNS predefined netif search — we register our own netif manually.
    # Without this, mdns_init() tries to register WiFi/ETH event handlers which
    # fail because no standard WiFi or Ethernet driver is active.
    add_idf_sdkconfig_option("CONFIG_MDNS_PREDEF_NETIF_STA", False)
    add_idf_sdkconfig_option("CONFIG_MDNS_PREDEF_NETIF_AP", False)
    add_idf_sdkconfig_option("CONFIG_MDNS_PREDEF_NETIF_ETH", False)

    # Include path for esp_netif internal header (needed for mDNS netif wrapper).
    # The esp_netif_lwip_internal.h header defines struct esp_netif_obj which we
    # need to create a minimal wrapper around mmipal's raw LWIP netif.
    esp_netif_internal = os.path.join(
        os.path.expanduser("~/.platformio"), "packages", "framework-espidf",
        "components", "esp_netif", "lwip"
    )
    if os.path.isdir(esp_netif_internal):
        cg.add_build_flag(f"-I{esp_netif_internal}")

    # Wrap network utility functions so ESPHome's API/OTA components
    # recognize halow as a valid network provider. Without this,
    # network::is_connected() returns false and API clients get disconnected.
    cg.add_build_flag("-Wl,--wrap=_ZN7esphome7network12is_connectedEv")
    cg.add_build_flag("-Wl,--wrap=_ZN7esphome7network16get_ip_addressesEv")
    cg.add_build_flag("-Wl,--wrap=_ZN7esphome7network15get_use_addressEv")

    # Wrap tcpip_input for RX byte counting — the SDK calls tcpip_input()
    # directly instead of going through netif->input
    cg.add_build_flag("-Wl,--wrap=tcpip_input")

    # PlatformIO extra script for firmware blob generation
    cg.add_platformio_option(
        "extra_scripts",
        [os.path.join(os.path.dirname(__file__), "pre_build.py")],
    )
