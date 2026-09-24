"""A successful build must not silently discard the requested radio configuration."""
import sys
from pathlib import Path

config = dict(line.split("=", 1) for line in Path(sys.argv[1]).read_text().splitlines()
              if line.startswith("CONFIG_") and "=" in line)
expected = {
    "ZMK_BLE": "y", "ZMK_SPLIT_ESB": "y", "ZMK_SPLIT_BLE": "n",
    "ZMK_SPLIT_ESB_USE_TIMESLOT": "y", "BT_LL_SOFTDEVICE": "y",
    "BT_LL_SW_SPLIT": "n", "MPSL_DYNAMIC_INTERRUPTS": "n",
    "MPSL_TIMESLOT_SESSION_COUNT": "2", "BT_MAX_CONN": "1", "BT_MAX_PAIRED": "1",
    "NRF_SECURITY": "n", "MBEDTLS_BUILTIN": "y", "BT_SMP": "y",
    "BT_SMP_SC_PAIR_ONLY": "y", "ZMK_SPLIT_ESB_PERIPHERAL_COUNT": "4",
    "ZMK_STUDIO": "y", "ZMK_USB": "y",
}
errors = []
for name, value in expected.items():
    actual = config.get("CONFIG_" + name, "n")
    print(f"{name}: {actual} (required {value})")
    if actual != value:
        errors.append(name)
if errors:
    raise SystemExit("Rejected diagnostic config: " + ", ".join(errors))
print("Configuration gate passed. This is NOT a hardware coexistence test.")
