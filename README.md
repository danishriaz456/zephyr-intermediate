# Zephyr Training Environment

Welcome to the Zephyr RTOS training! This repository includes a ready-to-use
development environment based on Zephyr **4.4.0**, which you can set up in one of
three ways:

---

## Manual Zephyr Setup

Follow the following guide:
- [Getting Started Guide](https://docs.zephyrproject.org/4.4.0/develop/getting_started/index.html#).

Make sure to select appropriate OS and to perform all steps till
[Build the Blinky Sample](https://docs.zephyrproject.org/4.4.0/develop/getting_started/index.html#build-the-blinky-sample).

## Lowered control thread priority value to meet 10 ms start latency

Define the 10 ms deadline as start latency (elapsed time from ready_ms
at timer expiry until the control thread dequeues the event) rather than
full processing time.

Root cause: In Zephyr, lower numerical priority values represent higher
urgency. With CONTROL_PRIORITY=7 and MAINTENANCE_PRIORITY=4, both
threads became ready on timer expiry, so maintenance ran first and held
the CPU in k_busy_wait(45000) for ~45 ms—visible between control_ready
and control_complete in the decoded CTF trace—causing 100% deadline
misses.

Fix: Select CONTROL_PRIORITY=2 (vs. 7) via #if/#elif so control
preempts maintenance immediately upon ISR exit, reducing start latency
from 45 ms to 0 ms with zero deadline misses.