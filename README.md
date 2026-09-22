# Touchless Industrial HMI: On-Device Gesture Recognition

An edge-native, contactless human-machine interface (HMI) built for high-throughput warehouse and industrial automation environments. The system enables operators handling heavy inventory or wearing gloves to pause processes, confirm steps, or navigate menus without physical screen interaction. 

Processing is performed 100% locally on-device without cloud or network dependencies, maintaining sub-300 ms response times while remaining resilient to dynamic environmental lighting and worker occlusions.

---

## Hardware Architecture

- **Compute Platform:** Arduino UNO Q (Dual-Core Architecture)
  - **Linux Application Core (Qualcomm Dragonwing QRB2210):** Runs embedded Linux, Video4Linux2 (V4L2) frame capture, and landmark-based edge inference.
  - **Real-Time I/O MCU (STM32U585):** Drives hardware peripherals and executes low-latency I2C transactions.
  - **Internal Bus:** On-chip UART/RPC bridge (`/dev/ttyHS0`) connecting Linux userspace to the real-time core.
- **Vision Capture:** Logitech BRIO 100 (UVC USB-C webcam stream at 640x480 @ 30 FPS).
- **Physical Feedback Interface:**
  - **Arduino Modulino Pixels:** 8 addressable RGB LEDs daisy-chained over I2C/Qwiic.
  - **Arduino Modulino Buzzer:** Piezo transducer driven via peripheral PWM over I2C/Qwiic.

---

## Gesture-to-Action Protocol (Industrial Andon Standard)

The Modulino Pixels follow standard IEC/NFPA industrial status conventions, providing unambiguous optical feedback visible at warehouse distances:

| Gesture | Industrial Intent | LED Status (Modulino Pixels) | RGB Value | Acoustic Feedback (Modulino Buzzer) |
| :--- | :--- | :--- | :--- | :--- |
| **Raised Palm** | **PAUSE / HOLD** | Solid Industrial Amber | `(255, 140, 0)` | Low Caution Tone (440 Hz, 250 ms) |
| **Thumbs Up** | **CONFIRM / ACK** | Solid Pure Green | `(0, 255, 0)` | Rising Chime (523 Hz -> 659 Hz) |
| **Swipe (L -> R)** | **SKIP / ADVANCE** | Process Cyan Chase | `(0, 220, 255)` | Rapid Pip (880 Hz, 80 ms) |
| **None / Out of Frame** | **MONITORING** | Dim Cobalt Baseline | `(0, 15, 80)` | Silent |
| **Unrecognized / Low Conf.** | **REJECT / FAULT** | Strobe Emergency Red | `(255, 0, 0)` | Error Buzz (220 Hz, 300 ms) |

---

## System Pipeline

```
 [Logitech BRIO 100]
        | (UVC Video / 30 FPS)
        v
 [Linux Application Core (QRB2210)]
   ├─ OpenCV V4L2 Ingestion
   ├─ Normalized Joint Topological Extraction
   └─ Debounced Gesture State Machine
        | (Internal UART Bridge / 115200 Baud)
        v
 [Real-Time MCU (STM32U585)]
   ├─ Non-blocking Command Deserialization
   └─ Modulino I2C Driver Engine
        | (Qwiic Bus: 3.3V / SDA / SCL / GND)
        +---> [Modulino Pixels] ---> [Modulino Buzzer]
```

---

## Repository Structure

```text
.
├── firmware/
│   └── uno_q_controller/
│       └── uno_q_controller.ino     # MCU firmware driving Modulino I2C bus
├── vision/
│   ├── vision_engine.py             # Video ingestion, landmark inference, IPC
│   └── requirements.txt             # Host Linux Python runtime dependencies
├── docs/
│   └── hardware_schematic.png       # Qwiic connection and pin routing topology
└── README.md
```

---

## Quick Start

### 1. MCU Firmware Flash
1. Connect the Arduino UNO Q to your workstation.
2. Open the Arduino IDE or App Lab, select the target board as **Arduino UNO Q (STM32 Core)**.
3. Install the **Modulino** library from the Arduino Library Manager.
4. Open and upload `firmware/uno_q_controller/uno_q_controller.ino`.

### 2. Linux Environment Configuration
Open an interactive shell on the UNO Q Debian environment:

```bash
# Update and install system-level vision dependencies
sudo apt update
sudo apt install -y python3-opencv python3-serial python3-pip

# Install runtime vision engine dependencies
pip3 install -r vision/requirements.txt --break-system-packages
```

### 3. Execution
Connect the Logitech BRIO 100 to the USB-C host port and run:

```bash
python3 vision/vision_engine.py
```

---

## Robustness & Edge Performance

- **Lighting Invariance:** The vision pipeline computes scale- and rotation-invariant topological angle vectors between skeletal hand landmarks rather than relying on raw RGB skin-segmentation thresholds, maintaining tracking fidelity through sudden ambient light shifts (e.g., loading dock doors opening).
- **Zero Network Exposure:** Inference, frame processing, and actuator triggers occur entirely on-device, satisfying strict zero-latency and air-gapped industrial requirements.
- **Failsafe Degradation:** When an operator moves out of frame or drops their hand, the state machine reverts actuator states to the dim cobalt monitoring baseline within 800 ms to prevent latched commands.