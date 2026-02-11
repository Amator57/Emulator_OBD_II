#pragma once

#include <Arduino.h>

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>OBD-II Emulator Control</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background-color: #f4f4f4; }
        h1 { color: #333; }
        form { background-color: #fff; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        label { font-weight: bold; display: block; margin-top: 10px;}
        input[type=text], input[type=number] { width: calc(100% - 22px); padding: 10px; margin-top: 5px; border: 1px solid #ccc; border-radius: 4px; }
        input[type=submit] { background-color: #4CAF50; color: white; padding: 12px 20px; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; margin-top: 20px;}
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
    </style>
</head>
<body>
    <div class="container">
        <h1>OBD-II Emulator Control</h1>
        
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

        <form id="updateForm" action="/update" method="get">
            <label for="vin">VIN:</label>
            <input type="text" id="vin" name="vin" value="VIN_NOT_SET" maxlength="17">

            <label for="rpm">Engine RPM:</label>
            <input type="number" id="rpm" name="rpm" value="1500">

            <label for="temp">Engine Temp (C):</label>
            <input type="number" id="temp" name="temp" value="90">

            <label for="speed">Vehicle Speed (km/h):</label>
            <input type="number" id="speed" name="speed" value="60">

            <label for="maf">MAF Rate (g/s):</label>
            <input type="number" id="maf" name="maf" step="0.1" value="10.0">

            <label for="fuel">Fuel Level (%):</label>
            <input type="number" id="fuel" name="fuel" step="0.1" value="75.0" min="0" max="100">

            <label for="dist_mil">Distance with MIL (km):</label>
            <input type="number" id="dist_mil" name="dist_mil" value="0" min="0">

            <label for="dtc_list">DTCs (comma-separated, e.g., P0123,C0456):</label>
            <input type="text" id="dtc_list" name="dtc_list" placeholder="P0101,C0300,B1000">

            <input type="submit" id="submitBtn" value="Update Emulator Data">
        </form>
        <button id="clearDtcBtn" class="button-red">Clear All DTCs</button>
        <button id="cycleBtn" class="button-blue">Simulate Driving Cycle</button>
        <div id="status" style="margin-top: 15px; font-weight: bold; text-align: center; min-height: 1.2em;"></div>

        <div class="live-status">
            <h2>Live Status</h2>
            <p><strong>VIN:</strong> <span id="status_vin">N/A</span></p>
            <p><strong>RPM:</strong> <span id="status_rpm">N/A</span></p>
            <p><strong>Temp:</strong> <span id="status_temp">N/A</span> &deg;C</p>
            <p><strong>Speed:</strong> <span id="status_speed">N/A</span> km/h</p>
            <p><strong>MAF:</strong> <span id="status_maf">N/A</span> g/s</p>
            <p><strong>Fuel Level:</strong> <span id="status_fuel">N/A</span> %</p>
            <p><strong>Distance w/ MIL:</strong> <span id="status_dist_mil">N/A</span> km</p>
            <p><strong>Error-Free Cycles:</strong> <span id="status_cycles">N/A</span></p>
            <p><strong>DTCs:</strong> <span id="status_dtcs">N/A</span></p>
            <p><strong>Permanent DTCs:</strong> <span id="status_permanent_dtcs">N/A</span></p>
        </div>
    </div>

    <script>
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

        let gateway = `ws://${window.location.hostname}/ws`;
        let websocket;

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
            
            // Оновлюємо блок "Live Status"
            document.getElementById('status_vin').textContent = data.vin;
            document.getElementById('status_rpm').textContent = data.rpm;
            document.getElementById('status_temp').textContent = data.temp;
            document.getElementById('status_speed').textContent = data.speed;
            document.getElementById('status_maf').textContent = data.maf;
            document.getElementById('status_fuel').textContent = data.fuel;
            document.getElementById('status_dist_mil').textContent = data.dist_mil;
            document.getElementById('status_cycles').textContent = data.cycles;
            document.getElementById('status_dtcs').textContent = data.dtcs.length > 0 ? data.dtcs.join(', ') : 'None';
            document.getElementById('status_permanent_dtcs').textContent = (data.permanent_dtcs && data.permanent_dtcs.length > 0) ? data.permanent_dtcs.join(', ') : 'None';

            if (data.dynamic_rpm !== undefined) {
                document.getElementById('dynamic_rpm_check').checked = data.dynamic_rpm;
            }

            if (data.misfire_sim !== undefined) {
                document.getElementById('misfire_sim_check').checked = data.misfire_sim;
            }

            // Синхронізуємо поля форми
            document.getElementById('vin').value = data.vin;
            document.getElementById('rpm').value = data.rpm;
            document.getElementById('temp').value = data.temp;
            document.getElementById('speed').value = data.speed;
            document.getElementById('maf').value = data.maf;
            document.getElementById('fuel').value = data.fuel;
            document.getElementById('dist_mil').value = data.dist_mil;
            document.getElementById('dtc_list').value = data.dtcs.join(',');
        }

        window.addEventListener('load', initWebSocket);
    </script>
</body>
</html>
)rawliteral";