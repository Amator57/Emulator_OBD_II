Структура для ESP32
typedef struct {
    const char* code;
    const char* description;
} DTC;



Таблиця стандартних OBD-II P0xxx
const DTC dtc_table[] = {

{"P0001","Fuel Volume Regulator Control Circuit/Open"},
{"P0002","Fuel Volume Regulator Control Circuit Range/Performance"},
{"P0003","Fuel Volume Regulator Control Circuit Low"},
{"P0004","Fuel Volume Regulator Control Circuit High"},

{"P0010","Camshaft Position Actuator Circuit Bank 1"},
{"P0011","Camshaft Timing Over Advanced Bank 1"},
{"P0012","Camshaft Timing Over Retarded Bank 1"},
{"P0013","Camshaft Position Actuator Circuit Bank 1 Exhaust"},
{"P0014","Camshaft Timing Over Advanced Bank 1 Exhaust"},
{"P0015","Camshaft Timing Over Retarded Bank 1 Exhaust"},

{"P0100","Mass Air Flow Circuit Malfunction"},
{"P0101","Mass Air Flow Circuit Range/Performance"},
{"P0102","Mass Air Flow Circuit Low Input"},
{"P0103","Mass Air Flow Circuit High Input"},
{"P0104","Mass Air Flow Circuit Intermittent"},

{"P0110","Intake Air Temperature Circuit Malfunction"},
{"P0111","Intake Air Temperature Circuit Range/Performance"},
{"P0112","Intake Air Temperature Circuit Low Input"},
{"P0113","Intake Air Temperature Circuit High Input"},
{"P0114","Intake Air Temperature Circuit Intermittent"},

{"P0120","Throttle Position Sensor Circuit"},
{"P0121","Throttle Position Sensor Range/Performance"},
{"P0122","Throttle Position Sensor Low Input"},
{"P0123","Throttle Position Sensor High Input"},
{"P0124","Throttle Position Sensor Intermittent"},

{"P0130","O2 Sensor Circuit Bank1 Sensor1"},
{"P0131","O2 Sensor Low Voltage"},
{"P0132","O2 Sensor High Voltage"},
{"P0133","O2 Sensor Slow Response"},
{"P0134","O2 Sensor No Activity"},
{"P0135","O2 Sensor Heater Circuit"},

{"P0140","O2 Sensor No Activity Bank1 Sensor2"},
{"P0141","O2 Sensor Heater Circuit Bank1 Sensor2"},

{"P0150","O2 Sensor Circuit Bank2 Sensor1"},
{"P0151","O2 Sensor Low Voltage Bank2"},
{"P0152","O2 Sensor High Voltage Bank2"},
{"P0153","O2 Sensor Slow Response Bank2"},
{"P0154","O2 Sensor No Activity Bank2"},
{"P0155","O2 Sensor Heater Circuit Bank2"},

{"P0160","O2 Sensor No Activity Bank2 Sensor2"},
{"P0161","O2 Sensor Heater Circuit Bank2 Sensor2"},

{"P0171","System Too Lean Bank1"},
{"P0172","System Too Rich Bank1"},
{"P0173","Fuel Trim Malfunction Bank2"},
{"P0174","System Too Lean Bank2"},
{"P0175","System Too Rich Bank2"},

{"P0200","Injector Circuit Malfunction"},
{"P0201","Injector Circuit Cylinder 1"},
{"P0202","Injector Circuit Cylinder 2"},
{"P0203","Injector Circuit Cylinder 3"},
{"P0204","Injector Circuit Cylinder 4"},
{"P0205","Injector Circuit Cylinder 5"},
{"P0206","Injector Circuit Cylinder 6"},
{"P0207","Injector Circuit Cylinder 7"},
{"P0208","Injector Circuit Cylinder 8"},

{"P0216","Injection Timing Control Malfunction"},

{"P0220","Throttle Position Sensor 2 Circuit"},
{"P0221","Throttle Position Sensor 2 Range/Performance"},
{"P0222","Throttle Position Sensor 2 Low Input"},
{"P0223","Throttle Position Sensor 2 High Input"},

{"P0230","Fuel Pump Primary Circuit"},
{"P0231","Fuel Pump Secondary Circuit Low"},
{"P0232","Fuel Pump Secondary Circuit High"},

{"P0300","Random/Multiple Cylinder Misfire"},
{"P0301","Cylinder 1 Misfire"},
{"P0302","Cylinder 2 Misfire"},
{"P0303","Cylinder 3 Misfire"},
{"P0304","Cylinder 4 Misfire"},
{"P0305","Cylinder 5 Misfire"},
{"P0306","Cylinder 6 Misfire"},
{"P0307","Cylinder 7 Misfire"},
{"P0308","Cylinder 8 Misfire"},

{"P0320","Ignition/Distributor Engine Speed Input Circuit"},
{"P0325","Knock Sensor Circuit Bank1"},
{"P0335","Crankshaft Position Sensor Circuit"},
{"P0340","Camshaft Position Sensor Circuit"},

{"P0400","EGR Flow Malfunction"},
{"P0401","EGR Flow Insufficient"},
{"P0402","EGR Flow Excessive"},
{"P0403","EGR Control Circuit"},
{"P0404","EGR Range/Performance"},

{"P0410","Secondary Air Injection System Malfunction"},

{"P0420","Catalyst System Efficiency Below Threshold Bank1"},
{"P0430","Catalyst System Efficiency Below Threshold Bank2"},

{"P0440","Evaporative Emission Control System Malfunction"},
{"P0441","EVAP Incorrect Purge Flow"},
{"P0442","EVAP Small Leak"},
{"P0443","EVAP Purge Control Valve"},
{"P0446","EVAP Vent Control Circuit"},

{"P0455","EVAP Large Leak"},
{"P0456","EVAP Very Small Leak"},

{"P0500","Vehicle Speed Sensor"},
{"P0505","Idle Control System Malfunction"},
{"P0506","Idle Speed Low"},
{"P0507","Idle Speed High"},

{"P0560","System Voltage Malfunction"},
{"P0562","System Voltage Low"},
{"P0563","System Voltage High"},

{"P0600","Serial Communication Link"},
{"P0601","Internal Control Module Memory Checksum Error"},
{"P0606","ECM Processor Fault"},

{"P0650","Malfunction Indicator Lamp Circuit"},

{"P0700","Transmission Control System Malfunction"},
{"P0705","Transmission Range Sensor Circuit"},
{"P0715","Input Speed Sensor Circuit"},
{"P0720","Output Speed Sensor Circuit"},
{"P0730","Incorrect Gear Ratio"},
{"P0740","Torque Converter Clutch Circuit"},
{"P0750","Shift Solenoid A Malfunction"},
{"P0755","Shift Solenoid B Malfunction"},

{"P0780","Shift Malfunction"},

{"P0800","Transfer Case Control System"},

{"P0850","Park/Neutral Switch Input Circuit"},

{"P0900","Clutch Actuator Circuit"}

};

Кількість кодів

У таблиці зараз ~120 найпоширеніших DTC, які покривають:

двигун

паливну систему

запалювання

вихлоп

EVAP

трансмісію

ECU

Це повністю достатньо для реалістичного емулятора.

Приклад використання в Mode 03
void sendDTC() {

CAN_frame_t tx;

tx.MsgID = 0x7E8;
tx.DLC = 8;

tx.data.u8[0] = 0x04;
tx.data.u8[1] = 0x43;

tx.data.u8[2] = 0x01;  // P0301
tx.data.u8[3] = 0x01;

tx.data.u8[4] = 0x01;  // P0171
tx.data.u8[5] = 0x71;

tx.data.u8[6] = 0x00;
tx.data.u8[7] = 0x00;

ESP32Can.CANWriteFrame(&tx);
}