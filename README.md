# CNC_welding_control_feedforward
This Arduino code establishes a feedforward control link between a CNC machine’s spindle ON signal and a welding arm. Instead of activating the spindle motor for drilling/milling, the signal is intercepted and repurposed to turn ON a welding torch that has been mounted in place of the spindle.

The signal from the CNC (typically 24V or 5V, stepped down via an optocoupler or voltage divider) is read via detectPin, and controls a relay or transistor circuit via weldingPin.
