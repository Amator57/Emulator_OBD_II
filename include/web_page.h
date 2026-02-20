#pragma once

#include <Arduino.h>

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>OBD-II Emulator-A Control</title>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background-color: #f4f4f4; }
        h1 { color: #333; }
        h2 { margin-top: 0; color: #333; border-bottom: 2px solid #eee; padding-bottom: 10px; margin-bottom: 20px;}
        label { font-weight: bold; display: block; margin-top: 10px;}
        input[type=text], input[type=number] { width: calc(100% - 22px); padding: 10px; margin-top: 5px; border: 1px solid #ccc; border-radius: 4px; }
        input[type=submit] { background-color: #4CAF50; color: white; padding: 12px 20px; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; margin-top: 20px;}
        .formula { font-size: 0.8em; color: #666; display: block; margin-top: -2px; margin-bottom: 10px; font-weight: normal; }
        nav { background-color: #333; overflow: hidden; border-radius: 8px 8px 0 0; }
        .tab-button { background-color: inherit; float: left; border: none; outline: none; cursor: pointer; padding: 14px 16px; transition: 0.3s; font-size: 16px; color: white; }
        .tab-button:hover { background-color: #555; }
        .tab-button.active { background-color: #2196F3; }
        .tab-button:disabled { background-color: #111; color: #888; cursor: not-allowed; }
        .page-content { display: none; padding: 20px; background-color: #fff; border-radius: 0 0 8px 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); animation: fadeEffect 0.5s; }
        @keyframes fadeEffect { from {opacity: 0;} to {opacity: 1;} }
        button, input[type=submit] { padding: 12px 20px; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; margin-top: 10px;}
        button:disabled, input[type=submit]:disabled { background-color: #cccccc; cursor: not-allowed; }
        input[type=submit]:hover { background-color: #45a049; }
        .button-red { background-color: #f44336; color: white; }
        .button-red:hover { background-color: #da190b; }
        .button-blue { background-color: #2196F3; color: white; }
        .button-blue:hover { background-color: #0b7dda; }
        .live-status { margin-top: 20px; padding: 20px; background-color: #e9f7ef; border-radius: 8px; border: 1px solid #a7d7c5; }
        .live-status h2 { margin-top: 0; color: #333; }
        .live-status p { margin: 5px 0; }
        .live-status span { font-weight: normal; color: #555; }
        .container { max-width: 600px; margin: auto; }
        /* Toggle Switch CSS */
        .switch { position: relative; display: inline-block; width: 50px; height: 24px; vertical-align: middle; margin-right: 10px; }
        .switch input { opacity: 0; width: 0; height: 0; }
        .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; transition: .4s; border-radius: 34px; }
        .slider:before { position: absolute; content: ""; height: 16px; width: 16px; left: 4px; bottom: 4px; background-color: white; transition: .4s; border-radius: 50%; }
        input:checked + .slider { background-color: #2196F3; }
        input:focus + .slider { box-shadow: 0 0 1px #2196F3; }
        input:checked + .slider:before { transform: translateX(26px); }
        canvas { background-color: #fff; border: 1px solid #ccc; border-radius: 4px; width: 100%; height: 200px; margin-top: 10px; }
        #can_log_area { width: 100%; height: 300px; font-family: monospace; font-size: 12px; border: 1px solid #ccc; overflow-y: scroll; background: #f9f9f9; padding: 5px; box-sizing: border-box; }
    </style>
</head>
<body>
    <div class="container">
        <h1>OBD-II Emulator-A Control</h1>

        <nav>
            <button class="tab-button" onclick="showPage('page-general', this)">General & DTC</button>
            <button class="tab-button" onclick="showPage('page-pids01', this)">PIDs 01-1F</button>
            <button class="tab-button" onclick="showPage('page-pids20', this)">PIDs 20-3F</button>
            <button class="tab-button" onclick="showPage('page-pids40', this)">PIDs 40-5F</button>
            <button class="tab-button" onclick="showPage('page-pids60', this)">PIDs 60-7F</button>
            <button class="tab-button" onclick="showPage('page-mode06', this)">Mode 06</button>
            <button class="tab-button" onclick="showPage('page-tcm', this)">TCM</button>
            <button class="tab-button" onclick="showPage('page-live', this)">Live Data</button>
            <button class="tab-button" onclick="showPage('page-faults', this)">Fault Injection</button>
            <button class="tab-button" onclick="showPage('page-can', this)">CAN Monitor</button>
        </nav>

        <form id="updateForm" action="/update" method="get">
            <div id="page-general" class="page-content">
                <h2>General Settings & DTC</h2>
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
                <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-bottom: 15px;">
                    <label style="display: flex; align-items: center;">
                        <label class="switch">
                            <input type="checkbox" id="ecu0_en" name="ecu0_en" checked>
                            <span class="slider"></span>
                        </label>
                        <span>Enable ECM (Engine)</span>
                    </label>
                    <label style="display: flex; align-items: center;">
                        <label class="switch">
                            <input type="checkbox" id="ecu1_en" name="ecu1_en" checked>
                            <span class="slider"></span>
                        </label>
                        <span>Enable TCM (Trans)</span>
                    </label>
                </div>

                <label for="vin">VIN (PID 09 02):</label>
                <input type="text" id="vin" name="vin" value="VIN_NOT_SET" maxlength="17">

                <label for="cal_id">Calibration ID (PID 09 04):</label>
                <input type="text" id="cal_id" name="cal_id" value="EMULATOR_CAL_ID" maxlength="16">

                <label for="cvn">CVN (PID 09 06):</label>
                <input type="text" id="cvn" name="cvn" value="A1B2C3D4" maxlength="8">

                <label for="dtc_list">DTCs (comma-separated, e.g., P0123,C0456):</label>
                <input type="text" id="dtc_list" name="dtc_list" placeholder="P0101,C0300,B1000">

                <label for="voltage">Battery Voltage (V):</label>
                <input type="number" id="voltage" name="voltage" step="0.1" value="14.2">

                <label for="dist_mil">Distance with MIL (km) (PID 0x31):</label>
                <span class="formula">Formula: A*256 + B</span>
                <input type="number" id="dist_mil" name="dist_mil" value="0" min="0">

                <input type="submit" id="submitBtn" value="Update Emulator Data">
                <button type="button" id="clearDtcBtn" class="button-red">Clear All DTCs</button>
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

                <label for="temp">Engine Temp (C) (PID 0x05):</label>
                <span class="formula">Formula: A - 40</span>
                <input type="number" id="temp" name="temp" value="90">

                <label for="speed">Vehicle Speed (km/h) (PID 0x0D):</label>
                <span class="formula">Formula: A</span>
                <input type="number" id="speed" name="speed" value="60">

                <label for="maf">MAF Rate (g/s) (PID 0x10):</label>
                <span class="formula">Formula: (A*256+B)/100</span>
                <input type="number" id="maf" name="maf" step="0.1" value="10.0">

                <label for="fuel_pressure">Fuel Pressure (kPa) (PID 0x0A):</label>
                <span class="formula">Formula: A * 3</span>
                <input type="number" id="fuel_pressure" name="fuel_pressure" value="350">

                <label for="timing">Timing Advance (deg) (PID 0x0E):</label>
                <span class="formula">Formula: (A-128)/2</span>
                <input type="number" id="timing" name="timing" step="0.5" value="5.0">
            </div>

            <div id="page-pids20" class="page-content">
                <h2>Mode 01 PIDs [20-3F]</h2>
                <label for="fuel">Fuel Level (%) (PID 0x2F):</label>
                <span class="formula">Formula: A * 100/255</span>
                <input type="number" id="fuel" name="fuel" step="0.1" value="75.0" min="0" max="100">
            </div>

            <div id="page-pids40" class="page-content">
                <h2>Mode 01 PIDs [40-5F]</h2>
                <label for="fuel_rate">Engine Fuel Rate (L/h) (PID 0x5E):</label>
                <span class="formula">Formula: ((A*256)+B)/20</span>
                <input type="number" id="fuel_rate" name="fuel_rate" step="0.1" value="1.5">
            </div>

            <div id="page-pids60" class="page-content">
                <h2>Mode 01 PIDs [60-7F]</h2>
                <p>Наразі в цьому діапазоні немає параметрів, що налаштовуються.</p>
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
            </div>

            <div id="page-tcm" class="page-content">
                <h2>Transmission Control Module (TCM)</h2>
                <label for="tcm_gear">Current Gear (PID 0xA4 - Custom):</label>
                <span class="formula">Value: 0=Neutral, 1-6=Gears, 255=Reverse</span>
                <input type="number" id="tcm_gear" name="tcm_gear" value="1" min="0" max="255">
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
                <button type="button" onclick="document.getElementById('can_log_area').innerHTML = ''">Clear Log</button>
            </div>
        </form>

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
            
            const url = form.action + '?' + params.toString();

            const submitBtn = document.getElementById('submitBtn');
            const statusDiv = document.getElementById('status');
            
            submitBtn.value = 'Updating...';
            submitBtn.disabled = true;
            statusDiv.textContent = '';

            fetch(url)
                .then(response => response.text())
                .then(data => {
                    statusDiv.textContent = data;
                    statusDiv.style.color = 'green';
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

        function toggleDynamicRPM(cb) {
            fetch('/update?dynamic_rpm=' + (cb.checked ? 'true' : 'false'));
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
                    document.getElementById('dtc_list').value = ''; // Очищаємо поле вводу DTC
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

        let gateway = `ws://${window.location.hostname}/ws`;
        let websocket;

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
            document.getElementById(id).addEventListener('change', updateChartSettings);
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
            document.getElementById('status_gear').textContent = data.tcm_gear;
            document.getElementById('status_temp').textContent = data.temp;
            document.getElementById('status_speed').textContent = data.speed;
            document.getElementById('status_maf').textContent = Number(data.maf).toFixed(1);
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

            // Синхронізуємо поля форми
            document.getElementById('vin').value = data.vin;
            document.getElementById('cal_id').value = data.cal_id;
            document.getElementById('cvn').value = data.cvn;
            document.getElementById('rpm').value = data.rpm;
            document.getElementById('tcm_gear').value = data.tcm_gear;
            document.getElementById('temp').value = data.temp;
            document.getElementById('speed').value = data.speed;
            document.getElementById('maf').value = Number(data.maf).toFixed(1);
            document.getElementById('timing').value = Number(data.timing).toFixed(1);
            document.getElementById('fuel_pressure').value = data.fuel_pressure;
            document.getElementById('fuel_rate').value = Number(data.fuel_rate).toFixed(1);
            document.getElementById('fuel').value = Number(data.fuel).toFixed(1);
            document.getElementById('dist_mil').value = data.dist_mil;
            document.getElementById('voltage').value = Number(data.voltage).toFixed(1);
            document.getElementById('dtc_list').value = data.dtcs.join(',');
            
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

        window.addEventListener('load', function() {
            // Show the first tab by default
            showPage('page-general', document.querySelector('.tab-button'));
            resizeCanvas();
            initWebSocket();
        });
    </script>
</body>
</html>
)rawliteral";
