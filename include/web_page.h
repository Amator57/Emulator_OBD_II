#pragma once

#include <Arduino.h>

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>OBD-II Emulator-A Control</title>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <script src="https://cdn.tailwindcss.com"></script>
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600&display=swap" rel="stylesheet">
    <style>
        body { font-family: 'Inter', sans-serif; background-color: #fafafa; }
        .page-content { display: none; }
        .page-content.active { display: block; animation: fadeIn 0.2s ease-in-out; }
        @keyframes fadeIn { from { opacity: 0; transform: translateY(4px); } to { opacity: 1; transform: translateY(0); } }
        .tab-button.active { background-color: white; color: black; box-shadow: 0 1px 3px rgba(0,0,0,0.1); }
        .shadcn-card { background: white; border: 1px solid #e4e4e7; border-radius: 0.5rem; box-shadow: 0 1px 2px rgba(0,0,0,0.05); }
        .shadcn-input { border: 1px solid #e4e4e7; border-radius: 0.375rem; padding: 0.5rem 0.75rem; font-size: 0.875rem; transition: ring 0.2s; }
        .shadcn-input:focus { outline: none; ring: 2px solid #000; ring-offset: 2px; }
        .shadcn-btn { display: inline-flex; align-items: center; justify-content: center; border-radius: 0.375rem; font-weight: 500; padding: 0.5rem 1rem; transition: background 0.2s; }
        .shadcn-btn-primary { background: #18181b; color: white; }
        .shadcn-btn-primary:hover { background: #27272a; }
        .shadcn-btn-outline { border: 1px solid #e4e4e7; background: white; }
        .shadcn-btn-outline:hover { background: #f4f4f5; }
        .switch-input:checked + .switch-slider { background-color: #18181b; }
    </style>
</head>
<body class="p-4 md:p-8">
    <div class="max-w-4xl mx-auto">
        <header class="mb-8">
            <h1 class="text-3xl font-bold tracking-tight">OBD-II Emulator-A</h1>
            <p class="text-muted-foreground text-gray-500">Advanced ECU Simulation Environment</p>
        </header>

        <nav class="flex p-1 space-x-1 bg-gray-100 rounded-lg mb-6 overflow-x-auto">
            <button class="tab-button" onclick="showPage('page-general', this)">General & DTC</button>
            <button class="tab-button" onclick="showPage('page-pids01', this)">PIDs 01-1F</button>
            <button class="tab-button" onclick="showPage('page-pids20', this)">PIDs 20-3F</button>
            <button class="tab-button" onclick="showPage('page-pids40', this)">PIDs 40-5F</button>
            <button class="tab-button" onclick="showPage('page-pids60', this)">PIDs 60-7F</button>
            <button class="tab-button" onclick="showPage('page-mode02', this)">Mode 02 FF</button>
            <button class="tab-button" onclick="showPage('page-mode03', this)">Mode 03/07/0A</button>
            <button class="tab-button" onclick="showPage('page-mode04', this)">Mode 04</button>
            <button class="tab-button" onclick="showPage('page-mode05', this)">Mode 05</button>
            <button class="tab-button" onclick="showPage('page-mode06', this)">Mode 06</button>
            <button class="tab-button" onclick="showPage('page-mode08', this)">Mode 08</button>
            <button class="tab-button" onclick="showPage('page-mode09', this)">Mode 09</button>
            <button class="tab-button" onclick="showPage('page-tcm', this)">TCM</button>
            <button class="tab-button" onclick="showPage('page-abs', this)">ABS</button>
            <button class="tab-button" onclick="showPage('page-srs', this)">SRS</button>
            <button class="tab-button" onclick="showPage('page-live', this)">Live Data</button>
            <button class="tab-button" onclick="showPage('page-faults', this)">Fault Injection</button>
            <button class="tab-button" onclick="showPage('page-can', this)">CAN Monitor</button>
            <button class="tab-button" onclick="showPage('page-network', this)">Network</button>
            <button class="tab-button px-3 py-1.5 text-sm font-medium rounded-md text-gray-600 hover:text-gray-900 transition-all" onclick="showPage('page-network', this)">Network</button>
        </nav>

        <form id="updateForm" action="/update" method="get">
            <div id="page-general" class="page-content active shadcn-card p-6">
                <h2 class="text-xl font-semibold mb-4">General Settings</h2>
                <div class="space-y-4">
                <div class="p-4 bg-purple-50 border border-purple-100 rounded-lg">
                    <label for="can_mode_select" class="block text-sm font-medium mb-1">CAN Protocol & Bitrate:</label>
                    <select id="can_mode_select" onchange="updateCanMode(this)" style="width: 100%; padding: 8px; margin-top: 5px; border-radius: 4px;">
                        <option value="1_500000">ISO 15765-4 CAN (11-bit ID, 500 kbaud)</option>
                        <option value="2_500000">ISO 15765-4 CAN (29-bit ID, 500 kbaud)</option>
                        <option value="1_250000">ISO 15765-4 CAN (11-bit ID, 250 kbaud)</option>
                        <option value="2_250000">ISO 15765-4 CAN (29-bit ID, 250 kbaud)</option>
                    </select>
                </div>
                <div style="margin-bottom: 15px; padding: 10px; background-color: #e3f2fd; border-radius: 8px; border: 1px solid #90caf9;">
                    <label style="display: flex; align-items: center;">
                        <label class="switch">
                            <input type="checkbox" id="dynamic_rpm_check" onchange="toggleDynamicRPM(this)">
                            <span class="slider"></span>
                        </label>
                        <span>Enable Dynamic RPM Emulation (Sine Wave)</span>
                    </label>
                </div>
                <div style="margin-bottom: 15px; padding: 10px; background-color: #ffebee; border-radius: 8px; border: 1px solid #ef9a9a;">
                    <label style="display: flex; align-items: center;">
                        <label class="switch">
                            <input type="checkbox" id="misfire_sim_check" onchange="toggleMisfireSim(this)">
                            <span class="slider"></span>
                        </label>
                        <span>Enable Misfire Simulation (P0300 at >3500 RPM)</span>
                    </label>
                </div>
                <div style="margin-bottom: 15px; padding: 10px; background-color: #fff3e0; border-radius: 8px; border: 1px solid #ffcc80;">
                    <label style="display: flex; align-items: center;">
                        <label class="switch">
                            <input type="checkbox" id="lean_mixture_sim_check" onchange="toggleLeanMixtureSim(this)">
                            <span class="slider"></span>
                        </label>
                        <span>Enable Lean Mixture Sim (P0171 at Low Fuel Pressure)</span>
                    </label>
                </div>
                
                <h3>ECU Control</h3>
                <div style="display: grid; grid-template-columns: 1fr 1fr 1fr 1fr; gap: 10px; margin-bottom: 15px;">
                    <label style="display: flex; flex-direction: column; align-items: center; text-align: center;">
                        <span style="min-height: 40px; display: flex; align-items: center; justify-content: center;">Enable ECM (Engine)</span>
                        <label class="switch" style="margin-top: 5px;">
                            <input type="checkbox" id="ecu0_en" name="ecu0_en" checked>
                            <span class="slider"></span>
                        </label>
                    </label>
                    <label style="display: flex; flex-direction: column; align-items: center; text-align: center;">
                        <span style="min-height: 40px; display: flex; align-items: center; justify-content: center;">Enable TCM (Trans)</span>
                        <label class="switch" style="margin-top: 5px;">
                            <input type="checkbox" id="ecu1_en" name="ecu1_en" checked>
                            <span class="slider"></span>
                        </label>
                    </label>
                    <label style="display: flex; flex-direction: column; align-items: center; text-align: center;">
                        <span style="min-height: 40px; display: flex; align-items: center; justify-content: center;">Enable ABS</span>
                        <label class="switch" style="margin-top: 5px;">
                            <input type="checkbox" id="ecu2_en" name="ecu2_en" checked>
                            <span class="slider"></span>
                        </label>
                    </label>
                    <label style="display: flex; flex-direction: column; align-items: center; text-align: center;">
                        <span style="min-height: 40px; display: flex; align-items: center; justify-content: center;">Enable SRS</span>
                        <label class="switch" style="margin-top: 5px;">
                            <input type="checkbox" id="ecu3_en" name="ecu3_en" checked>
                            <span class="slider"></span>
                        </label>
                    </label>
                </div>

                <input type="submit" id="submitBtn" value="Update Emulator Data">
                <button type="button" id="cycleBtn" class="button-blue">Simulate Driving Cycle</button>
                <div id="status" style="margin-top: 15px; font-weight: bold; text-align: center; min-height: 1.2em;"></div>

                <div style="margin-top: 20px; padding-top: 20px; border-top: 2px solid #eee;">
                    <h2>Configuration Management</h2>
                    <button type="button" id="saveConfigBtn" class="button-blue" style="margin-right: 10px;">Save Configuration</button>
                    <button type="button" id="saveNvsBtn" class="button-blue" style="margin-right: 10px;">Save to Device</button>
                    <div style="margin-top: 15px; display: inline-block;">
                        <input type="file" id="loadConfigFile" accept=".json" style="width: auto;">
                        <button type="button" id="loadConfigBtn">Load Configuration</button>
                    </div>
                </div>
            </div>

            <div id="page-pids01" class="page-content">
                <h2>Mode 01 PIDs [01-1F]</h2>
                <label for="rpm">Engine RPM (PID 0x0C):</label>
                <span class="formula">Formula: (A*256+B)/4</span>
                <input type="number" id="rpm" name="rpm" value="1500">

                <label for="fuel_sys">Fuel System Status (PID 0x03):</label>
                <span class="formula">Value (Dec): 2=Closed Loop, 1=Open Loop, 512=CL (Sys1)</span>
                <input type="number" id="fuel_sys" name="fuel_sys" value="512">

                <label for="load">Engine Load (%) (PID 0x04):</label>
                <span class="formula">Formula: A * 100/255</span>
                <input type="number" id="load" name="load" step="0.1" value="35.0">

                <label for="temp">Engine Temp (C) (PID 0x05):</label>
                <span class="formula">Formula: A - 40</span>
                <input type="number" id="temp" name="temp" value="90">

                <label for="speed">Vehicle Speed (km/h) (PID 0x0D):</label>
                <span class="formula">Formula: A</span>
                <input type="number" id="speed" name="speed" value="60">

                <label for="map">MAP (kPa) (PID 0x0B):</label>
                <span class="formula">Formula: A</span>
                <input type="number" id="map" name="map" value="40">

                <label for="iat">Intake Air Temp (C) (PID 0x0F):</label>
                <span class="formula">Formula: A - 40</span>
                <input type="number" id="iat" name="iat" value="30">

                <label for="maf">MAF Rate (g/s) (PID 0x10):</label>
                <span class="formula">Formula: (A*256+B)/100</span>
                <input type="number" id="maf" name="maf" step="0.1" value="10.0">

                <label for="tps">Throttle Position (%) (PID 0x11):</label>
                <span class="formula">Formula: A * 100/255</span>
                <input type="number" id="tps" name="tps" step="0.1" value="15.0">

                <label for="stft">Short Term Fuel Trim (%) (PID 0x06):</label>
                <span class="formula">Formula: (A-128)*100/128</span>
                <input type="number" id="stft" name="stft" step="0.1" value="0.0">

                <label for="ltft">Long Term Fuel Trim (%) (PID 0x07):</label>
                <span class="formula">Formula: (A-128)*100/128</span>
                <input type="number" id="ltft" name="ltft" step="0.1" value="2.5">

                <label for="stft2">Short Term Fuel Trim B2 (%) (PID 0x08):</label>
                <span class="formula">Formula: (A-128)*100/128</span>
                <input type="number" id="stft2" name="stft2" step="0.1" value="0.0">

                <label for="ltft2">Long Term Fuel Trim B2 (%) (PID 0x09):</label>
                <span class="formula">Formula: (A-128)*100/128</span>
                <input type="number" id="ltft2" name="ltft2" step="0.1" value="2.5">

                <label for="o2">O2 Sensor B1S1 (V) (PID 0x14):</label>
                <span class="formula">Formula: A/200</span>
                <input type="number" id="o2" name="o2" step="0.01" value="0.45">

                <label for="obd_std">OBD Standard (PID 0x1C):</label>
                <span class="formula">Value: 1=OBD-II, 6=EOBD, etc.</span>
                <input type="number" id="obd_std" name="obd_std" value="1">

                <label for="o2_sens">O2 Sensors Present (PID 0x1D):</label>
                <span class="formula">Bitmask: Bank1(0-3), Bank2(4-7)</span>
                <input type="number" id="o2_sens" name="o2_sens" value="3">

                <label for="fuel_pressure">Fuel Pressure (kPa) (PID 0x0A):</label>
                <span class="formula">Formula: A * 3</span>
                <input type="number" id="fuel_pressure" name="fuel_pressure" value="350">

                <label for="timing">Timing Advance (deg) (PID 0x0E):</label>
                <span class="formula">Formula: (A-128)/2</span>
                <input type="number" id="timing" name="timing" step="0.5" value="5.0">
                <button type="button" class="button-blue" onclick="updateAndShow(1)">Apply Changes</button>
            </div>

            <div id="page-pids20" class="page-content">
                <h2>Mode 01 PIDs [20-3F]</h2>
                <label for="fuel">Fuel Level (%) (PID 0x2F):</label>
                <span class="formula">Formula: A * 100/255</span>
                <input type="number" id="fuel" name="fuel" step="0.1" value="75.0" min="0" max="100">

                <label for="dist_since_clear">Dist. Since Codes Cleared (km) (PID 0x31):</label>
                <span class="formula">Formula: A*256 + B</span>
                <input type="number" id="dist_since_clear" name="dist_since_clear" value="0" min="0">

                <label for="dist_mil_on">Dist. Traveled MIL On (km) (PID 0x21):</label>
                <span class="formula">Formula: A*256 + B</span>
                <input type="number" id="dist_mil_on" name="dist_mil_on" value="0" min="0">

                <label for="evap">Evap Purge (%) (PID 0x2E):</label>
                <span class="formula">Formula: A * 100/255</span>
                <input type="number" id="evap" name="evap" step="0.1" value="0.0">

                <label for="egr_cmd">Commanded EGR (%) (PID 0x2C):</label>
                <span class="formula">Formula: A * 100/255</span>
                <input type="number" id="egr_cmd" name="egr_cmd" step="0.1" value="0.0">

                <label for="egr_err">EGR Error (%) (PID 0x2D):</label>
                <span class="formula">Formula: (A-128) * 100/128</span>
                <input type="number" id="egr_err" name="egr_err" step="0.1" value="0.0">

                <label for="evap_vp">EVAP Vapor Pressure (Pa) (PID 0x32):</label>
                <span class="formula">Formula: (A*256+B)/4</span>
                <input type="number" id="evap_vp" name="evap_vp" value="0">

                <label for="warm_ups">Warm-ups Since Cleared (PID 0x30):</label>
                <span class="formula">Formula: A</span>
                <input type="number" id="warm_ups" name="warm_ups" value="10">

                <label for="baro">Barometric Pressure (kPa) (PID 0x33):</label>
                <span class="formula">Formula: A</span>
                <input type="number" id="baro" name="baro" value="100">

                <label for="fuel_rail_pres_rel">Fuel Rail Pressure (relative) (kPa) (PID 0x22):</label>
                <span class="formula">Formula: (A*256+B)*0.079</span>
                <input type="number" id="fuel_rail_pres_rel" name="fuel_rail_pres_rel" value="300">

                <label for="fuel_rail_pres_gauge">Fuel Rail Pressure (gauge) (kPa) (PID 0x23):</label>
                <span class="formula">Formula: (A*256+B)*10</span>
                <input type="number" id="fuel_rail_pres_gauge" name="fuel_rail_pres_gauge" value="4000">

                <h3>Wideband O2 Sensors</h3>
                <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 10px;">
                    <div>
                        <label>B1S1 Lambda (PID 34): <input type="number" id="wb_b1s1_l" name="wb_b1s1_l" step="0.01" value="1.0"></label>
                        <label>B1S1 mA: <input type="number" id="wb_b1s1_c" name="wb_b1s1_c" step="0.1" value="0.0"></label>
                    </div>
                    <div>
                        <label>B1S2 Lambda (PID 35): <input type="number" id="wb_b1s2_l" name="wb_b1s2_l" step="0.01" value="1.0"></label>
                        <label>B1S2 mA: <input type="number" id="wb_b1s2_c" name="wb_b1s2_c" step="0.1" value="0.0"></label>
                    </div>
                    <div>
                        <label>B2S1 Lambda (PID 36): <input type="number" id="wb_b2s1_l" name="wb_b2s1_l" step="0.01" value="1.0"></label>
                        <label>B2S1 mA: <input type="number" id="wb_b2s1_c" name="wb_b2s1_c" step="0.1" value="0.0"></label>
                    </div>
                    <div>
                        <label>B2S2 Lambda (PID 37): <input type="number" id="wb_b2s2_l" name="wb_b2s2_l" step="0.01" value="1.0"></label>
                        <label>B2S2 mA: <input type="number" id="wb_b2s2_c" name="wb_b2s2_c" step="0.1" value="0.0"></label>
                    </div>
                </div>

                <h3>Catalyst Temperatures (C)</h3>
                <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 10px;">
                    <label>Bank 1 Sensor 1 (PID 3C): <input type="number" id="cat_b1s1" name="cat_b1s1" value="400"></label>
                    <label>Bank 2 Sensor 1 (PID 3D): <input type="number" id="cat_b2s1" name="cat_b2s1" value="400"></label>
                    <label>Bank 1 Sensor 2 (PID 3E): <input type="number" id="cat_b1s2" name="cat_b1s2" value="350"></label>
                    <label>Bank 2 Sensor 2 (PID 3F): <input type="number" id="cat_b2s2" name="cat_b2s2" value="350"></label>
                </div>
                <button type="button" class="button-blue" onclick="updateAndShow(1)">Apply Changes</button>
            </div>

            <div id="page-pids40" class="page-content">
                <h2>Mode 01 PIDs [40-5F]</h2>
                <label for="voltage_pid">Control Module Voltage (V) (PID 0x42):</label>
                <span class="formula">Formula: (A*256+B)/1000</span>
                <input type="number" id="voltage_pid" name="voltage" step="0.1" value="14.2">

                <label for="fuel_rate">Engine Fuel Rate (L/h) (PID 0x5E):</label>
                <span class="formula">Formula: ((A*256)+B)/20</span>
                <input type="number" id="fuel_rate" name="fuel_rate" step="0.1" value="1.5">

                <label for="evap_abs">Abs EVAP Pressure (kPa) (PID 0x53):</label>
                <span class="formula">Formula: (A*256+B)/200</span>
                <input type="number" id="evap_abs" name="evap_abs" step="0.1" value="100.0">

                <label for="abs_load">Absolute Load (%) (PID 0x43):</label>
                <span class="formula">Formula: (A*256+B)*100/255</span>
                <input type="number" id="abs_load" name="abs_load" step="0.1" value="20.0">

                <label for="lambda">Commanded Equivalence Ratio (PID 0x44):</label>
                <span class="formula">Formula: (A*256+B)/32768</span>
                <input type="number" id="lambda" name="lambda" step="0.01" value="1.00">

                <label for="rel_tps">Relative Throttle Pos (%) (PID 0x45):</label>
                <span class="formula">Formula: A*100/255</span>
                <input type="number" id="rel_tps" name="rel_tps" step="0.1" value="10.0">

                <label for="cmd_throttle">Commanded Throttle Actuator (%) (PID 0x4C):</label>
                <span class="formula">Formula: A*100/255</span>
                <input type="number" id="cmd_throttle" name="cmd_throttle" step="0.1" value="15.0">

                <label for="rel_app">Relative Accel Pedal Pos (%) (PID 0x5A):</label>
                <span class="formula">Formula: A*100/255</span>
                <input type="number" id="rel_app" name="rel_app" step="0.1" value="0.0">

                <label for="app_d">Accel Pedal Pos D (%) (PID 0x49):</label>
                <span class="formula">Formula: A*100/255</span>
                <input type="number" id="app_d" name="app_d" step="0.1" value="15.0">

                <label for="app_e">Accel Pedal Pos E (%) (PID 0x4A):</label>
                <span class="formula">Formula: A*100/255</span>
                <input type="number" id="app_e" name="app_e" step="0.1" value="15.0">

                <label for="time_mil">Time Run with MIL On (min) (PID 0x4D):</label>
                <span class="formula">Formula: A*256 + B</span>
                <input type="number" id="time_mil" name="time_mil" value="0">

                <label for="time_clear">Time Since DTC Cleared (min) (PID 0x4E):</label>
                <span class="formula">Formula: A*256 + B</span>
                <input type="number" id="time_clear" name="time_clear" value="0">

                <label for="amb_temp">Ambient Air Temp (C) (PID 0x46):</label>
                <span class="formula">Formula: A-40</span>
                <input type="number" id="amb_temp" name="amb_temp" value="20">

                <label for="oil_temp">Engine Oil Temp (C) (PID 0x5C):</label>
                <span class="formula">Formula: A-40</span>
                <input type="number" id="oil_temp" name="oil_temp" value="85">
                <button type="button" class="button-blue" onclick="updateAndShow(1)">Apply Changes</button>
            </div>

            <div id="page-pids60" class="page-content">
                <h2>Mode 01 PIDs [60-7F]</h2>
                <p>Наразі в цьому діапазоні немає параметрів, що налаштовуються.</p>
            </div>

            <div id="page-mode02" class="page-content">
                <h2>Mode 02 - Freeze Frame</h2>
                <p>Configure the data returned in Mode 02. These values are usually captured when a DTC occurs.</p>
                
                <label for="ff_rpm">Freeze Frame RPM:</label>
                <input type="number" id="ff_rpm" name="ff_rpm" value="0">

                <label for="ff_speed">Freeze Frame Speed (km/h):</label>
                <input type="number" id="ff_speed" name="ff_speed" value="0">

                <label for="ff_temp">Freeze Frame Temp (C):</label>
                <input type="number" id="ff_temp" name="ff_temp" value="0">

                <label for="ff_maf">Freeze Frame MAF (g/s):</label>
                <input type="number" id="ff_maf" name="ff_maf" step="0.1" value="0.0">

                <label for="ff_pres">Freeze Frame Fuel Pressure (kPa):</label>
                <input type="number" id="ff_pres" name="ff_pres" value="0">

                <button type="button" class="button-blue" onclick="updateAndShow(5)">Apply Changes</button>
            </div>

            <div id="page-mode03" class="page-content">
                <h2>Mode 03 / 07 / 0A - DTCs</h2>
                <p>Use the arrows to construct a DTC and press "Add DTC" or Enter key.</p>
                <div id="dtc-input-container">
                    <div class="dtc-char">
                        <button type="button" class="arrow-up" onclick="changeDTCChar(0, 1)">&#9650;</button>
                        <span id="dtc-char-0">P</span>
                        <button type="button" class="arrow-down" onclick="changeDTCChar(0, -1)">&#9660;</button>
                    </div>
                    <div class="dtc-char">
                        <button type="button" class="arrow-up" onclick="changeDTCChar(1, 1)">&#9650;</button>
                        <span id="dtc-char-1">0</span>
                        <button type="button" class="arrow-down" onclick="changeDTCChar(1, -1)">&#9660;</button>
                    </div>
                    <div class="dtc-char">
                        <button type="button" class="arrow-up" onclick="changeDTCChar(2, 1)">&#9650;</button>
                        <span id="dtc-char-2">1</span>
                        <button type="button" class="arrow-down" onclick="changeDTCChar(2, -1)">&#9660;</button>
                    </div>
                    <div class="dtc-char">
                        <button type="button" class="arrow-up" onclick="changeDTCChar(3, 1)">&#9650;</button>
                        <span id="dtc-char-3">0</span>
                        <button type="button" class="arrow-down" onclick="changeDTCChar(3, -1)">&#9660;</button>
                    </div>
                    <div class="dtc-char">
                        <button type="button" class="arrow-up" onclick="changeDTCChar(4, 1)">&#9650;</button>
                        <span id="dtc-char-4">1</span>
                        <button type="button" class="arrow-down" onclick="changeDTCChar(4, -1)">&#9660;</button>
                    </div>
                    <button type="button" id="addDtcBtn" class="button-blue" style="margin-left: 10px; height: 100%;">Add DTC</button>
                </div>
                <div id="dtc-list-container">
                    <h3>Injected DTCs</h3>
                    <ul id="dtc-list-ui"></ul>
                </div>
                <input type="hidden" id="dtc_list" name="dtc_list">
                <p><i>Note: Adding a DTC here will populate Current (Mode 03), Pending (Mode 07), and Permanent (Mode 0A) lists. Press "Apply Changes" to send to emulator.</i></p>
                <button type="button" class="button-blue" onclick="updateAndShow(6)">Apply Changes</button>
            </div>

            <div id="page-mode04" class="page-content">
                <h2>Mode 04 - Clear Diagnostic Information</h2>
                <p>Pressing the button below will clear all DTCs and Freeze Frame data.</p>
                <button type="button" id="clearDtcBtn" class="button-red">Clear All DTCs (Mode 04)</button>
            </div>

            <div id="page-mode05" class="page-content">
                <h2>Mode 05 - O2 Sensor Monitoring</h2>
                <p>Mode 05 is not supported in CAN OBD-II (replaced by Mode 06).</p>
                <button type="button" class="button-blue" onclick="updateAndShow(8)">Apply Changes</button>
            </div>

            <div id="page-mode06" class="page-content">
                <h2>Mode 06 Test Results (for Engine ECU)</h2>
                <p>Configure on-board monitoring test results. Enter TID in hex (e.g., 21).</p>
                <div style="display: grid; grid-template-columns: 1fr 1fr 1fr 1fr; gap: 10px; align-items: center;">
                    <b>Test ID (Hex)</b><b>Value</b><b>Min Limit</b><b>Max Limit</b>
                    <input type="text" name="m06_t1_id" placeholder="e.g., 21">
                    <input type="number" name="m06_t1_val" value="0">
                    <input type="number" name="m06_t1_min" value="0">
                    <input type="number" name="m06_t1_max" value="65535">
                    <input type="text" name="m06_t2_id" placeholder="e.g., 22">
                    <input type="number" name="m06_t2_val" value="0">
                    <input type="number" name="m06_t2_min" value="0">
                    <input type="number" name="m06_t2_max" value="65535">
                </div>
                <button type="button" class="button-blue" onclick="updateAndShow(2)">Apply Changes</button>
            </div>

            <div id="page-mode08" class="page-content">
                <h2>Mode 08 - On-Board Control</h2>
                <p>Control of on-board system, test or component is not currently implemented.</p>
            </div>

            <div id="page-mode09" class="page-content">
                <h2>Mode 09 - Vehicle Information</h2>
                <label for="vin">VIN (PID 09 02):</label>
                <input type="text" id="vin" name="vin" value="VIN_NOT_SET" maxlength="17">
                <span class="formula">Note: Characters I, O, and Q are not allowed in VIN.</span>

                <label for="cal_id">Calibration ID (PID 09 04):</label>
                <input type="text" id="cal_id" name="cal_id" value="ECM_CAL_ID_V1" maxlength="16">

                <label for="cvn">CVN (PID 09 06):</label>
                <input type="text" id="cvn" name="cvn" value="ECE1E2E3" maxlength="8">
                <button type="button" class="button-blue" onclick="updateAndShow(7)">Apply Changes</button>
            </div>

            <div id="page-tcm" class="page-content">
                <h2>Transmission Control Module (TCM)</h2>
                <label for="tcm_gear">Current Gear (PID 0xA4 - Custom):</label>
                <span class="formula">Value: 0=Neutral, 1-6=Gears, 255=Reverse</span>
                <input type="number" id="tcm_gear" name="tcm_gear" value="1" min="0" max="255">
                <button type="button" class="button-blue" onclick="updateAndShow(3)">Apply Changes</button>
            </div>

            <div id="page-abs" class="page-content">
                <h2>Anti-lock Braking System (ABS)</h2>
                <label for="abs_speed">Wheel Speed (km/h) (PID 0x0D):</label>
                <input type="number" id="abs_speed" name="abs_speed" value="60">
                
                <label for="abs_vin">VIN (PID 09 02):</label>
                <input type="text" id="abs_vin" name="abs_vin" value="123EMULATORVINABS" maxlength="17">
                <button type="button" class="button-blue" onclick="updateAndShow(9)">Apply Changes</button>
            </div>

            <div id="page-srs" class="page-content">
                <h2>Supplemental Restraint System (SRS)</h2>
                <label for="srs_vin">VIN (PID 09 02):</label>
                <input type="text" id="srs_vin" name="srs_vin" value="123EMULATORVINSRS" maxlength="17">
                <button type="button" class="button-blue" onclick="updateAndShow(10)">Apply Changes</button>
            </div>

            <div id="page-faults" class="page-content">
                <h2>Fault Injection & Stress Testing</h2>
                
                <div style="margin-bottom: 15px; padding: 10px; background-color: #ffebee; border-radius: 8px; border: 1px solid #ef9a9a;">
                    <label>Response Delay (ms): <span id="val_delay">0</span></label>
                    <input type="range" id="frame_delay" name="frame_delay" min="0" max="1000" value="0" oninput="document.getElementById('val_delay').innerText = this.value">
                    
                    <label>Random CAN Error Rate (%): <span id="val_error">0</span></label>
                    <input type="range" id="error_rate" name="error_rate" min="0" max="100" value="0" oninput="document.getElementById('val_error').innerText = this.value">
                </div>

                <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 10px;">
                    <label style="display: flex; align-items: center;">
                        <label class="switch">
                            <input type="checkbox" id="fault_silent" name="fault_silent" value="true">
                            <span class="slider"></span>
                        </label>
                        <span>Silent ECU (No Response)</span>
                    </label>
                    <label style="display: flex; align-items: center;">
                        <label class="switch">
                            <input type="checkbox" id="fault_multi" name="fault_multi" value="true">
                            <span class="slider"></span>
                        </label>
                        <span>Multiple ECU Respond (Ghost)</span>
                    </label>
                    <label style="display: flex; align-items: center;">
                        <label class="switch">
                            <input type="checkbox" id="fault_seq" name="fault_seq" value="true">
                            <span class="slider"></span>
                        </label>
                        <span>Incorrect Sequence Number (ISO-TP)</span>
                    </label>
                    <label style="display: flex; align-items: center;">
                        <label class="switch">
                            <input type="checkbox" id="fault_stmin" name="fault_stmin" value="true">
                            <span class="slider"></span>
                        </label>
                        <span>STmin Overflow (Ignore Flow Control)</span>
                    </label>
                    <label style="display: flex; align-items: center;">
                        <label class="switch">
                            <input type="checkbox" id="fault_wrong_fc" name="fault_wrong_fc" value="true">
                            <span class="slider"></span>
                        </label>
                        <span>Wrong Flow Control (Send WAIT)</span>
                    </label>
                    <label style="display: flex; align-items: center;">
                        <label class="switch">
                            <input type="checkbox" id="fault_partial_vin" name="fault_partial_vin" value="true">
                            <span class="slider"></span>
                        </label>
                        <span>Partial Multi-frame (Timeout)</span>
                    </label>
                </div>
                
                <h3>Quick Faults</h3>
                <button type="button" class="button-blue" onclick="updateAndShow(4)">Apply Changes</button>
                <button type="button" class="button-red" onclick="injectRandomDTC()">Inject Random DTC</button>
            </div>
            
            <div id="page-can" class="page-content">
                <h2>CAN Bus Monitor</h2>
                <label style="display: flex; align-items: center; margin-bottom: 10px;">
                    <label class="switch">
                        <input type="checkbox" id="can_log" name="can_log">
                        <span class="slider"></span>
                    </label>
                    <span>Enable CAN Logging (Real-time)</span>
                </label>
                <div id="can_log_area"></div>
                <button type="button" class="button-blue" onclick="updateAndShow(4)">Apply Changes</button>
                <button type="button" onclick="document.getElementById('can_log_area').innerHTML = ''">Clear Log</button>
            </div>
        </form>

        <div id="page-network" class="page-content">
            <h2>Wi-Fi Configuration</h2>
            <p>Configure the emulator to connect to your local Wi-Fi network. If connection fails, it will revert to Access Point mode.</p>
            <form onsubmit="submitWifi(event)">
                <label for="wifi_ssid">SSID (Network Name):</label>
                <input type="text" id="wifi_ssid" name="ssid" placeholder="MyWiFiNetwork" required>
                
                <label for="wifi_pass">Password:</label>
                <input type="text" id="wifi_pass" name="pass" placeholder="Password">
                
                <input type="submit" value="Save & Connect" class="button-blue">
            </form>
        </div>

        <div id="page-live" class="page-content">
            <div style="margin-top: 0;">
                <h2>Live Chart</h2>
                <div style="margin-bottom: 10px; font-size: 14px;">
                    <label style="display:inline; margin-right:5px; color:#2196F3">RPM Max: <input type="number" id="chart_max_rpm" value="6000" style="width:50px;"></label>
                    <label style="display:inline; margin-right:5px; color:#4CAF50">Speed Max: <input type="number" id="chart_max_speed" value="200" style="width:40px;"></label>
                    <label style="display:inline; margin-right:5px; color:#f44336">Temp Max: <input type="number" id="chart_max_temp" value="150" style="width:40px;"></label>
                    <label style="display:inline; margin-right:5px; color:#FF9800">MAF Max: <input type="number" id="chart_max_maf" value="100" style="width:40px;"></label>
                    <label style="display:inline; margin-right:5px; color:#9C27B0">Timing Max: <input type="number" id="chart_max_timing" value="60" style="width:40px;"></label>
                    <label style="display:inline; color:#00BCD4">Fuel Max: <input type="number" id="chart_max_fuel" value="20" style="width:40px;"></label>
                </div>
                <canvas id="rpmChart"></canvas>
            </div>

            <div class="live-status">
                <h2>Live Status</h2>
                <p><strong>VIN:</strong> <span id="status_vin">N/A</span></p>
                <p><strong>CAL ID:</strong> <span id="status_cal_id">N/A</span></p>
                <p><strong>CVN:</strong> <span id="status_cvn">N/A</span></p>
                <p><strong>RPM:</strong> <span id="status_rpm">N/A</span></p>
                <p><strong>Gear:</strong> <span id="status_gear">N/A</span></p>
                <p><strong>Temp:</strong> <span id="status_temp">N/A</span> &deg;C</p>
                <p><strong>Speed:</strong> <span id="status_speed">N/A</span> km/h</p>
                <p><strong>MAF:</strong> <span id="status_maf">N/A</span> g/s</p>
                <p><strong>Load:</strong> <span id="status_load">N/A</span> %</p>
                <p><strong>MAP:</strong> <span id="status_map">N/A</span> kPa</p>
                <p><strong>TPS:</strong> <span id="status_tps">N/A</span> %</p>
                <p><strong>IAT:</strong> <span id="status_iat">N/A</span> &deg;C</p>
                <p><strong>STFT:</strong> <span id="status_stft">N/A</span> %</p>
                <p><strong>LTFT:</strong> <span id="status_ltft">N/A</span> %</p>
                <p><strong>O2 B1S1:</strong> <span id="status_o2">N/A</span> V</p>
                <p><strong>Timing Adv:</strong> <span id="status_timing">N/A</span> deg</p>
                <p><strong>Fuel Rate:</strong> <span id="status_fuel_rate">N/A</span> L/h</p>
                <p><strong>Fuel Level:</strong> <span id="status_fuel">N/A</span> %</p>
                <p><strong>Distance w/ MIL:</strong> <span id="status_dist_mil">N/A</span> km</p>
                <p><strong>Error-Free Cycles:</strong> <span id="status_cycles">N/A</span></p>
                <p><strong>Voltage:</strong> <span id="status_voltage">N/A</span> V</p>
                <p><strong>DTCs:</strong> <span id="status_dtcs">N/A</span></p>
                <p><strong>Permanent DTCs:</strong> <span id="status_permanent_dtcs">N/A</span></p>
            </div>

            <div class="live-status" style="margin-top: 20px; background-color: #e8eaf6; border-color: #c5cae9;">
                <h2>UDS Status (ECM)</h2>
                <p><strong>Session:</strong> <span id="status_uds_session">Default</span></p>
                <p><strong>Security Access:</strong> <span id="status_uds_security">Locked</span></p>
            </div>

            <div id="freeze-frame-status" class="live-status" style="display: none; background-color: #fef9e7; border-color: #f1c40f; margin-top: 20px;">
                <h2>Freeze Frame Data</h2>
                <p><strong>Trigger DTC:</strong> <span id="status_ff_dtc">N/A</span></p>
                <p><strong>RPM:</strong> <span id="status_ff_rpm">N/A</span></p>
                <p><strong>Speed:</strong> <span id="status_ff_speed">N/A</span> km/h</p>
                <p><strong>Temp:</strong> <span id="status_ff_temp">N/A</span> &deg;C</p>
                <p><strong>MAF:</strong> <span id="status_ff_maf">N/A</span> g/s</p>
                <p><strong>Fuel Pressure:</strong> <span id="status_ff_fuel_pressure">N/A</span> kPa</p>
            </div>
        </div>

    </div>

    <script>
        function showPage(pageId, element) {
            document.querySelectorAll('.page-content').forEach(page => page.style.display = 'none');
            document.querySelectorAll('.tab-button').forEach(btn => btn.classList.remove('active'));
            
            const pageToShow = document.getElementById(pageId);
            pageToShow.style.display = 'block';
            if (element) {
                element.classList.add('active');
            }

            if (pageId === 'page-live') {
                resizeCanvas(); // Redraw chart when its tab is shown
            }
        }

        function updateAndShow(pageIndex) {
            const form = document.getElementById('updateForm');
            const params = new URLSearchParams();
            
            // Знаходимо активну вкладку (div з display: block)
            let activeDiv = null;
            document.querySelectorAll('.page-content').forEach(div => {
                if (div.style.display === 'block') {
                    activeDiv = div;
                }
            });

            if (activeDiv) {
                // Збираємо дані тільки з полів активної вкладки
                const inputs = activeDiv.querySelectorAll('input, select, textarea');
                inputs.forEach(el => {
                    if (el.name && !el.disabled) {
                        if (el.type === 'checkbox' || el.type === 'radio') {
                            // Надсилаємо 'true'/'false' для всіх чекбоксів на активній вкладці
                            params.append(el.name, el.checked ? 'true' : 'false');
                        } else {
                            params.append(el.name, el.value);
                        }
                    }
                });
            }
            
            // Завжди додаємо стан перемикачів ECU, щоб він не скидався при оновленні з інших вкладок
            params.set('ecu0_en', document.getElementById('ecu0_en').checked ? 'true' : 'false');
            params.set('ecu1_en', document.getElementById('ecu1_en').checked ? 'true' : 'false');
            params.set('ecu2_en', document.getElementById('ecu2_en').checked ? 'true' : 'false');
            params.set('ecu3_en', document.getElementById('ecu3_en').checked ? 'true' : 'false');
            
            params.append('page', pageIndex);
            
            const url = form.action + '?' + params.toString();
            const statusDiv = document.getElementById('status');
            statusDiv.textContent = 'Applying changes...';
            
            fetch(url)
                .then(response => response.json()) // Очікуємо JSON-відповідь
                .then(data => {
                    // Візуальна фіксація: змінюємо текст статусу на більш конкретний
                    statusDiv.textContent = (pageIndex === 6) ? 'DTCs Successfully Applied' : 'Changes Applied';
                    statusDiv.style.color = 'green';
                    onMessage({data: JSON.stringify(data)}); // Симулюємо повідомлення WebSocket для оновлення UI
                    
                    // Додаємо ефект "миготіння" для списку помилок
                    if (pageIndex === 6) {
                        const container = document.getElementById('dtc-list-container');
                        container.classList.add('flash-success');
                        setTimeout(() => container.classList.remove('flash-success'), 1000);
                    }
                })
                .catch(error => {
                    statusDiv.textContent = 'Error applying changes';
                    statusDiv.style.color = 'red';
                })
                .finally(() => {
                    setTimeout(() => { statusDiv.textContent = ''; }, 2000);
                });
        }

        function injectRandomDTC() {
            const codes = ['P0101', 'P0300', 'P0171', 'C0035', 'U0100'];
            const randomCode = codes[Math.floor(Math.random() * codes.length)];
            document.getElementById('dtc_list').value = randomCode;
            document.getElementById('submitBtn').click();
        }

        document.getElementById('updateForm').addEventListener('submit', function(event) {
            event.preventDefault(); // Запобігаємо перезавантаженню сторінки

            const form = event.target;
            const formData = new FormData(form);
            const params = new URLSearchParams();
            // Додаємо в запит тільки ті параметри, які мають значення
            for (const pair of formData) {
                if (pair[1]) {
                    params.append(pair[0], pair[1]);
                }
            }

            // Явно встановлюємо значення для всіх перемикачів ECU, щоб відправити 'true' або 'false'
            params.set('ecu0_en', document.getElementById('ecu0_en').checked ? 'true' : 'false');
            params.set('ecu1_en', document.getElementById('ecu1_en').checked ? 'true' : 'false');
            params.set('ecu2_en', document.getElementById('ecu2_en').checked ? 'true' : 'false');
            params.set('ecu3_en', document.getElementById('ecu3_en').checked ? 'true' : 'false');
            
            const url = form.action + '?' + params.toString();

            const submitBtn = document.getElementById('submitBtn');
            const statusDiv = document.getElementById('status');
            
            submitBtn.value = 'Updating...';
            submitBtn.disabled = true;
            statusDiv.textContent = '';

            fetch(url)
                .then(response => response.json())
                .then(data => {
                    statusDiv.textContent = 'General Settings Applied Successfully';
                    statusDiv.style.color = 'green';
                    onMessage({data: JSON.stringify(data)}); // Оновлюємо UI новими даними
                })
                .catch(error => {
                    statusDiv.textContent = 'Error: Could not connect to the server.';
                    statusDiv.style.color = 'red';
                })
                .finally(() => {
                    submitBtn.value = 'Update Emulator Data';
                    submitBtn.disabled = false;
                    setTimeout(() => { statusDiv.textContent = ''; }, 5000); // Очистити статус через 5 секунд
                });
        });

        function updateCanMode(select) {
            const parts = select.value.split('_');
            const mode = parts[0];
            const bitrate = parts[1];
            select.disabled = true;
            fetch('/update?mode=' + mode + '&bitrate=' + bitrate)
                .then(response => response.json())
                .then(data => {
                    document.getElementById('status').textContent = 'CAN Mode Updated. Re-initializing...';
                    document.getElementById('status').style.color = 'blue';
                    onMessage({data: JSON.stringify(data)});
                    setTimeout(() => { document.getElementById('status').textContent = ''; select.disabled = false; }, 2000);
                })
                .catch(err => { select.disabled = false; });
        }

        function toggleDynamicRPM(cb) {
            setSimulationMode(cb.checked);
            fetch('/update?dynamic_rpm=' + (cb.checked ? 'true' : 'false'));
        }

        function setSimulationMode(enabled) {
            const disabled = enabled;
            const submitBtn = document.getElementById('submitBtn');
            if (submitBtn) submitBtn.disabled = disabled;
            
            // Знаходимо та блокуємо всі кнопки "Apply Changes"
            const applyBtns = document.querySelectorAll('button[onclick^="updateAndShow"]');
            applyBtns.forEach(btn => {
                btn.disabled = disabled;
                // Змінюємо стиль курсору та прозорість для візуального ефекту
                btn.style.opacity = disabled ? "0.6" : "1.0";
                btn.style.cursor = disabled ? "not-allowed" : "pointer";
            });

            const addDtcBtn = document.getElementById('addDtcBtn');
            if (addDtcBtn) {
                addDtcBtn.disabled = disabled;
                addDtcBtn.style.opacity = disabled ? "0.6" : "1.0";
                addDtcBtn.style.cursor = disabled ? "not-allowed" : "pointer";
            }
        }

        function toggleMisfireSim(cb) {
            fetch('/update?misfire_sim=' + (cb.checked ? 'true' : 'false'));
        }

        function toggleLeanMixtureSim(cb) {
            fetch('/update?lean_mixture_sim=' + (cb.checked ? 'true' : 'false'));
        }

        document.getElementById('clearDtcBtn').addEventListener('click', function() {
            const btn = this;
            const statusDiv = document.getElementById('status');
            
            btn.textContent = 'Clearing...';
            btn.disabled = true;
            statusDiv.textContent = '';

            fetch('/clear_dtc')
                .then(response => response.text())
                .then(data => {
                    statusDiv.textContent = data;
                    statusDiv.style.color = 'blue';
                    // document.getElementById('dtc_list').value = ''; // Очищаємо поле вводу DTC
                    if (typeof renderDtcList === 'function') {
                        injectedDtcs = [];
                        renderDtcList();
                    }
                })
                .catch(error => {
                    statusDiv.textContent = 'Error: Could not connect to the server.';
                    statusDiv.style.color = 'red';
                })
                .finally(() => {
                    btn.textContent = 'Clear All DTCs';
                    btn.disabled = false;
                    setTimeout(() => { statusDiv.textContent = ''; }, 5000);
                });
        });

        document.getElementById('cycleBtn').addEventListener('click', function() {
            const btn = this;
            const statusDiv = document.getElementById('status');
            
            btn.disabled = true;
            statusDiv.textContent = 'Simulating cycle...';

            fetch('/cycle')
                .then(response => response.text())
                .then(data => {
                    statusDiv.textContent = data;
                    statusDiv.style.color = 'blue';
                })
                .catch(error => {
                    statusDiv.textContent = 'Error: Could not connect to the server.';
                    statusDiv.style.color = 'red';
                })
                .finally(() => {
                    btn.disabled = false;
                    setTimeout(() => { statusDiv.textContent = ''; }, 3000);
                });
        });

        document.getElementById('saveConfigBtn').addEventListener('click', function() {
            window.location.href = '/config.json';
        });

        document.getElementById('saveNvsBtn').addEventListener('click', function() {
            const btn = this;
            const statusDiv = document.getElementById('status');
            
            btn.disabled = true;
            statusDiv.textContent = 'Saving to device memory...';
            statusDiv.style.color = 'blue';

            fetch('/save_nvs')
                .then(response => response.text())
                .then(data => {
                    statusDiv.textContent = data;
                    statusDiv.style.color = 'green';
                })
                .catch(error => {
                    statusDiv.textContent = 'Error: Could not save to device.';
                    statusDiv.style.color = 'red';
                })
                .finally(() => {
                    btn.disabled = false;
                    setTimeout(() => { statusDiv.textContent = ''; }, 5000);
                });
        });

        document.getElementById('loadConfigBtn').addEventListener('click', function() {
            const fileInput = document.getElementById('loadConfigFile');
            const statusDiv = document.getElementById('status');

            if (fileInput.files.length === 0) {
                statusDiv.textContent = 'Please select a configuration file first.';
                statusDiv.style.color = 'red';
                return;
            }
            const file = fileInput.files[0];
            const reader = new FileReader();
            
            reader.onload = function(e) {
                const content = e.target.result;
                statusDiv.textContent = 'Loading configuration...';
                statusDiv.style.color = 'blue';

                fetch('/load_config', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: content
                })
                .then(response => response.text())
                .then(data => {
                    statusDiv.textContent = data;
                    statusDiv.style.color = 'green';
                    setTimeout(() => { statusDiv.textContent = ''; }, 5000);
                })
                .catch(error => {
                    statusDiv.textContent = 'Error loading configuration: ' + error;
                    statusDiv.style.color = 'red';
                });
            };
            reader.readAsText(file);
        });

        function submitWifi(e) {
            e.preventDefault();
            if(!confirm("Device will restart and try to connect to the new network. Continue?")) return;
            
            const ssid = document.getElementById('wifi_ssid').value;
            const pass = document.getElementById('wifi_pass').value;
            const formData = new FormData();
            formData.append("ssid", ssid);
            formData.append("pass", pass);

            fetch('/save_wifi', {
                method: 'POST',
                body: formData
            })
            .then(response => response.text())
            .then(data => {
                alert(data);
            })
            .catch(error => {
                alert("Error saving WiFi settings");
            });
        }

        let gateway = `ws://${window.location.hostname}/ws`;
        let websocket;

        let currentDtc = ['P', '0', '1', '0', '1'];
        const dtcChars = [
            ['P', 'C', 'B', 'U'],
            ['0', '1', '2', '3'],
            ['0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'],
            ['0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'],
            ['0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F']
        ];
        let injectedDtcs = [];

        function changeDTCChar(position, direction) {
            const charSet = dtcChars[position];
            let currentIndex = charSet.indexOf(currentDtc[position]);
            currentIndex += direction;
            if (currentIndex < 0) currentIndex = charSet.length - 1;
            if (currentIndex >= charSet.length) currentIndex = 0;
            currentDtc[position] = charSet[currentIndex];
            document.getElementById(`dtc-char-${position}`).textContent = currentDtc[position];
        }

        function renderDtcList() {
            const listElement = document.getElementById('dtc-list-ui');
            listElement.innerHTML = '';
            injectedDtcs.forEach((dtc, index) => {
                const li = document.createElement('li');
                li.innerHTML = `<span>${dtc}</span> <button type="button" class="remove-dtc" onclick="removeDtc(${index})">&times;</button>`;
                listElement.appendChild(li);
            });
            document.getElementById('dtc_list').value = injectedDtcs.join(',');
        }

        function addDtc() {
            const newDtc = currentDtc.join('');
            if (injectedDtcs.includes(newDtc)) { alert('DTC ' + newDtc + ' is already in the list.'); return; }
            if (injectedDtcs.length >= 8) { alert('Maximum number of DTCs (8) reached.'); return; }
            injectedDtcs.push(newDtc);
            renderDtcList();
        }

        function removeDtc(index) {
            injectedDtcs.splice(index, 1);
            renderDtcList();
        }

        // --- Chart Logic ---
        const canvas = document.getElementById('rpmChart');
        const ctx = canvas.getContext('2d');
        let speedHistory = new Array(60).fill(0); // Історія на 60 точок
        let rpmHistory = new Array(60).fill(0); // Історія на 60 точок
        let tempHistory = new Array(60).fill(0); // Історія на 60 точок
        let mafHistory = new Array(60).fill(0);
        let timingHistory = new Array(60).fill(0);
        let fuelRateHistory = new Array(60).fill(0);

        // Load chart settings
        const chartSettings = ['chart_max_rpm', 'chart_max_speed', 'chart_max_temp', 'chart_max_maf', 'chart_max_timing', 'chart_max_fuel'];
        chartSettings.forEach(id => {
            if(localStorage.getItem(id)) document.getElementById(id).value = localStorage.getItem(id);
            document.getElementById(id).addEventListener('input', updateChartSettings);
        });

        function updateChartSettings() {
            chartSettings.forEach(id => {
                localStorage.setItem(id, document.getElementById(id).value);
            });
            drawChart();
        }

        function resizeCanvas() {
            canvas.width = canvas.clientWidth;
            canvas.height = canvas.clientHeight;
            drawChart();
        }
        window.addEventListener('resize', resizeCanvas);

        function drawChart() {
            const w = canvas.width;
            const h = canvas.height;
            ctx.clearRect(0, 0, w, h);
            ctx.font = '12px Arial';

            const maxRPM = parseFloat(document.getElementById('chart_max_rpm').value) || 6000;
            const maxSpeed = parseFloat(document.getElementById('chart_max_speed').value) || 200;
            const maxTemp = parseFloat(document.getElementById('chart_max_temp').value) || 150;
            const maxMaf = parseFloat(document.getElementById('chart_max_maf').value) || 100;
            const maxTiming = parseFloat(document.getElementById('chart_max_timing').value) || 60;
            const maxFuel = parseFloat(document.getElementById('chart_max_fuel').value) || 20;
            
            const step = w / (rpmHistory.length - 1);

            function drawLine(history, maxVal, color) {
                ctx.beginPath();
                ctx.strokeStyle = color;
                ctx.lineWidth = 2;
                for (let i = 0; i < history.length; i++) {
                    let val = history[i];
                    let y = h - (val / maxVal * h);
                    if (y < 0) y = 0; // Clip top
                    if (y > h) y = h; // Clip bottom
                    if (i === 0) ctx.moveTo(0, y);
                    else ctx.lineTo(i * step, y);
                }
                ctx.stroke();
            }

            drawLine(rpmHistory, maxRPM, '#2196F3'); // Blue
            drawLine(speedHistory, maxSpeed, '#4CAF50'); // Green
            drawLine(tempHistory, maxTemp, '#f44336'); // Red
            drawLine(mafHistory, maxMaf, '#FF9800'); // Orange
            drawLine(timingHistory, maxTiming, '#9C27B0'); // Purple
            drawLine(fuelRateHistory, maxFuel, '#00BCD4'); // Cyan

            // --- Draw Legend ---
            let lx = 10;
            ctx.fillStyle = '#2196F3';
            ctx.fillText('RPM', lx, 15); lx += 40;
            ctx.fillStyle = '#4CAF50';
            ctx.fillText('Speed', lx, 15); lx += 50;
            ctx.fillStyle = '#f44336';
            ctx.fillText('Temp', lx, 15); lx += 40;
            ctx.fillStyle = '#FF9800';
            ctx.fillText('MAF', lx, 15); lx += 40;
            ctx.fillStyle = '#9C27B0';
            ctx.fillText('Timing', lx, 15); lx += 50;
            ctx.fillStyle = '#00BCD4';
            ctx.fillText('Fuel', lx, 15);
        }

        function initWebSocket() {
            console.log('Trying to open a WebSocket connection...');
            websocket = new WebSocket(gateway);
            websocket.onopen    = onOpen;
            websocket.onclose   = onClose;
            websocket.onmessage = onMessage;
        }

        function onOpen(event) {
            console.log('Connection opened');
        }

        function onClose(event) {
            console.log('Connection closed');
            setTimeout(initWebSocket, 2000); // Спробувати перепідключитися через 2 секунди
        }

        function onMessage(event) {
            const data = JSON.parse(event.data);
            
            if (data.log) {
                const logArea = document.getElementById('can_log_area');
                const line = document.createElement('div');
                line.textContent = new Date().toLocaleTimeString() + " " + data.log;
                logArea.appendChild(line);
                logArea.scrollTop = logArea.scrollHeight;
                return; // It's a log message, stop processing status
            }
            
            // Оновлюємо блок "Live Status"
            document.getElementById('status_vin').textContent = data.vin;
            document.getElementById('status_cal_id').textContent = data.cal_id;
            document.getElementById('status_cvn').textContent = data.cvn;
            document.getElementById('status_rpm').textContent = data.rpm;
            if(data.fuel_sys !== undefined) document.getElementById('fuel_sys').value = data.fuel_sys;
            document.getElementById('status_gear').textContent = data.tcm_gear;
            document.getElementById('status_temp').textContent = data.temp;
            document.getElementById('status_speed').textContent = data.speed;
            document.getElementById('status_maf').textContent = Number(data.maf).toFixed(1);
            
            document.getElementById('status_load').textContent = Number(data.load).toFixed(1);
            document.getElementById('status_map').textContent = data.map;
            document.getElementById('status_tps').textContent = Number(data.tps).toFixed(1);
            document.getElementById('status_iat').textContent = data.iat;
            document.getElementById('status_stft').textContent = Number(data.stft).toFixed(1);
            document.getElementById('status_ltft').textContent = Number(data.ltft).toFixed(1);
            document.getElementById('status_o2').textContent = Number(data.o2).toFixed(2);

            document.getElementById('status_timing').textContent = Number(data.timing).toFixed(1);
            document.getElementById('status_fuel_rate').textContent = Number(data.fuel_rate).toFixed(1);
            document.getElementById('status_fuel').textContent = Number(data.fuel).toFixed(1);
            document.getElementById('status_dist_mil').textContent = data.dist_mil;
            document.getElementById('status_cycles').textContent = data.cycles;
            document.getElementById('status_voltage').textContent = Number(data.voltage).toFixed(1);
            document.getElementById('status_dtcs').textContent = data.dtcs.length > 0 ? data.dtcs.join(', ') : 'None';
            document.getElementById('status_permanent_dtcs').textContent = (data.permanent_dtcs && data.permanent_dtcs.length > 0) ? data.permanent_dtcs.join(', ') : 'None';

            // Оновлюємо блок "UDS Status"
            document.getElementById('status_uds_session').textContent = data.uds_session === 3 ? 'Extended' : (data.uds_session === 1 ? 'Default' : 'Unknown');
            document.getElementById('status_uds_security').textContent = data.uds_security ? 'Unlocked (Level 1)' : 'Locked';

            const ff_div = document.getElementById('freeze-frame-status');
            if (data.freeze_frame_set) {
                ff_div.style.display = 'block';
                document.getElementById('status_ff_dtc').textContent = data.ff_dtc;
                document.getElementById('status_ff_rpm').textContent = data.ff_rpm;
                document.getElementById('status_ff_speed').textContent = data.ff_speed;
                document.getElementById('status_ff_temp').textContent = data.ff_temp;
                document.getElementById('status_ff_maf').textContent = data.ff_maf;
                document.getElementById('status_ff_fuel_pressure').textContent = data.ff_fuel_pressure;
            } else {
                ff_div.style.display = 'none';
            }

            if (data.misfire_sim !== undefined) {
                document.getElementById('misfire_sim_check').checked = data.misfire_sim;
            }

            if (data.lean_mixture_sim !== undefined) {
                document.getElementById('lean_mixture_sim_check').checked = data.lean_mixture_sim;
            }

            if (data.mode !== undefined && data.bitrate !== undefined) {
                const val = data.mode + '_' + data.bitrate;
                const select = document.getElementById('can_mode_select');
                if (select && document.activeElement !== select) {
                    select.value = val;
                }
            }
            
            if (data.dynamic_rpm !== undefined) {
                const dynCheck = document.getElementById('dynamic_rpm_check');
                if (dynCheck.checked !== data.dynamic_rpm) {
                    dynCheck.checked = data.dynamic_rpm;
                }
                setSimulationMode(data.dynamic_rpm);
            }

            // Синхронізуємо поля форми
            document.getElementById('vin').value = data.vin;
            document.getElementById('cal_id').value = data.cal_id;
            document.getElementById('cvn').value = data.cvn;
            document.getElementById('rpm').value = data.rpm;
            document.getElementById('tcm_gear').value = data.tcm_gear;
            document.getElementById('temp').value = data.temp;
            document.getElementById('speed').value = data.speed;
            document.getElementById('maf').value = Number(data.maf).toFixed(1);
            document.getElementById('load').value = Number(data.load).toFixed(1);
            document.getElementById('map').value = data.map;
            document.getElementById('tps').value = Number(data.tps).toFixed(1);
            document.getElementById('iat').value = data.iat;
            document.getElementById('stft').value = Number(data.stft).toFixed(1);
            document.getElementById('ltft').value = Number(data.ltft).toFixed(1);
            document.getElementById('stft2').value = Number(data.stft2).toFixed(1);
            document.getElementById('ltft2').value = Number(data.ltft2).toFixed(1);
            document.getElementById('o2').value = Number(data.o2).toFixed(2);
            if(data.obd_std !== undefined) document.getElementById('obd_std').value = data.obd_std;
            if(data.o2_sens !== undefined) document.getElementById('o2_sens').value = data.o2_sens;
            document.getElementById('timing').value = Number(data.timing).toFixed(1);
            document.getElementById('fuel_pressure').value = data.fuel_pressure;
            document.getElementById('fuel_rate').value = Number(data.fuel_rate).toFixed(1);
            document.getElementById('fuel').value = Number(data.fuel).toFixed(1);
            document.getElementById('dist_since_clear').value = data.dist_mil;
            document.getElementById('dist_mil_on').value = data.dist_mil_on;
            document.getElementById('evap').value = Number(data.evap).toFixed(1);
            if(data.egr_cmd !== undefined) document.getElementById('egr_cmd').value = Number(data.egr_cmd).toFixed(1);
            if(data.egr_err !== undefined) document.getElementById('egr_err').value = Number(data.egr_err).toFixed(1);
            if(data.evap_vp !== undefined) document.getElementById('evap_vp').value = data.evap_vp;
            if(data.evap_abs !== undefined) document.getElementById('evap_abs').value = Number(data.evap_abs).toFixed(1);
            document.getElementById('warm_ups').value = data.warm_ups;
            document.getElementById('baro').value = data.baro;
            if(data.fuel_rail_pres_rel !== undefined) document.getElementById('fuel_rail_pres_rel').value = data.fuel_rail_pres_rel;
            if(data.fuel_rail_pres_gauge !== undefined) document.getElementById('fuel_rail_pres_gauge').value = data.fuel_rail_pres_gauge;
            document.getElementById('wb_b1s1_l').value = Number(data.wb_b1s1_l).toFixed(2); document.getElementById('wb_b1s1_c').value = Number(data.wb_b1s1_c).toFixed(1);
            document.getElementById('wb_b1s2_l').value = Number(data.wb_b1s2_l).toFixed(2); document.getElementById('wb_b1s2_c').value = Number(data.wb_b1s2_c).toFixed(1);
            document.getElementById('wb_b2s1_l').value = Number(data.wb_b2s1_l).toFixed(2); document.getElementById('wb_b2s1_c').value = Number(data.wb_b2s1_c).toFixed(1);
            document.getElementById('wb_b2s2_l').value = Number(data.wb_b2s2_l).toFixed(2); document.getElementById('wb_b2s2_c').value = Number(data.wb_b2s2_c).toFixed(1);
            if(data.cat_b1s1 !== undefined) document.getElementById('cat_b1s1').value = data.cat_b1s1;
            if(data.cat_b2s1 !== undefined) document.getElementById('cat_b2s1').value = data.cat_b2s1;
            if(data.cat_b1s2 !== undefined) document.getElementById('cat_b1s2').value = data.cat_b1s2;
            if(data.cat_b2s2 !== undefined) document.getElementById('cat_b2s2').value = data.cat_b2s2;
            document.getElementById('abs_load').value = Number(data.abs_load).toFixed(1);
            document.getElementById('lambda').value = Number(data.lambda).toFixed(2);
            document.getElementById('rel_tps').value = Number(data.rel_tps).toFixed(1);
            if(data.cmd_throttle !== undefined) document.getElementById('cmd_throttle').value = Number(data.cmd_throttle).toFixed(1);
            if(data.rel_app !== undefined) document.getElementById('rel_app').value = Number(data.rel_app).toFixed(1);
            if(data.app_d !== undefined) document.getElementById('app_d').value = Number(data.app_d).toFixed(1);
            if(data.app_e !== undefined) document.getElementById('app_e').value = Number(data.app_e).toFixed(1);
            if(data.time_mil !== undefined) document.getElementById('time_mil').value = data.time_mil;
            if(data.time_clear !== undefined) document.getElementById('time_clear').value = data.time_clear;
            document.getElementById('amb_temp').value = data.amb_temp;
            document.getElementById('oil_temp').value = data.oil_temp;
            document.getElementById('voltage_pid').value = Number(data.voltage).toFixed(1);
            
            if (JSON.stringify(injectedDtcs) !== JSON.stringify(data.dtcs)) {
                injectedDtcs = data.dtcs || [];
                renderDtcList();
            }

            // ABS & SRS
            if(data.abs_speed !== undefined) document.getElementById('abs_speed').value = data.abs_speed;
            if(data.abs_vin !== undefined) document.getElementById('abs_vin').value = data.abs_vin;
            if(data.srs_vin !== undefined) document.getElementById('srs_vin').value = data.srs_vin;
            
            // Faults
            document.getElementById('frame_delay').value = data.frame_delay || 0;
            document.getElementById('val_delay').innerText = data.frame_delay || 0;
            document.getElementById('error_rate').value = data.error_rate || 0;
            document.getElementById('val_error').innerText = data.error_rate || 0;
            document.getElementById('fault_seq').checked = data.fault_seq;
            document.getElementById('fault_silent').checked = data.fault_silent;
            document.getElementById('fault_multi').checked = data.fault_multi;
            document.getElementById('fault_stmin').checked = data.fault_stmin;
            document.getElementById('fault_wrong_fc').checked = data.fault_wrong_fc;
            document.getElementById('fault_partial_vin').checked = data.fault_partial_vin;
            
            document.getElementById('ecu0_en').checked = data.ecu0_en;
            document.getElementById('ecu1_en').checked = data.ecu1_en;
            document.getElementById('ecu2_en').checked = data.ecu2_en;
            document.getElementById('ecu3_en').checked = data.ecu3_en;
            document.getElementById('can_log').checked = data.can_log;

            // Оновлення графіку
            speedHistory.push(data.speed);
            if (speedHistory.length > 60) speedHistory.shift();

            rpmHistory.push(data.rpm);
            if (rpmHistory.length > 60) rpmHistory.shift();

            tempHistory.push(data.temp);
            if (tempHistory.length > 60) tempHistory.shift();

            mafHistory.push(data.maf);
            if (mafHistory.length > 60) mafHistory.shift();

            timingHistory.push(data.timing);
            if (timingHistory.length > 60) timingHistory.shift();

            fuelRateHistory.push(data.fuel_rate);
            if (fuelRateHistory.length > 60) fuelRateHistory.shift();

            drawChart();
        }

        document.addEventListener('keydown', function(event) {
            // Check if the DTC page is active
            if (document.getElementById('page-mode03').style.display === 'block') {
                if (event.key === 'Enter') {
                    event.preventDefault(); // Prevent form submission if any
                    document.getElementById('addDtcBtn').click(); // Trigger the add button click
                }
            }
        });

        window.addEventListener('load', function() {
            // Show the first tab by default
            showPage('page-general', document.querySelector('.tab-button'));
            resizeCanvas();
            initWebSocket();
            // Init DTC input
            document.getElementById('addDtcBtn').addEventListener('click', addDtc);
            for(let i=0; i<5; i++) { document.getElementById(`dtc-char-${i}`).textContent = currentDtc[i]; }
        });
    </script>
</body>
</html>
)rawliteral";
