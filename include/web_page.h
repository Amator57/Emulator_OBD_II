#ifndef WEB_PAGE_H
#define WEB_PAGE_H

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>OBD-II Emulator</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    :root {
      --bg-color: #2c3e50;
      --text-color: #ecf0f1;
      --card-bg-color: #34495e;
      --primary-color: #3498db;
      --primary-color-hover: #2980b9;
      --input-bg-color: #2c3e50;
      --input-border-color: #95a5a6;
      --danger-color: #e74c3c;
    }
    body { 
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; 
      background-color: var(--bg-color); 
      color: var(--text-color);
      text-align: center; 
      margin: 0;
      padding: 20px;
    }
    h2, h4 {
      color: var(--text-color);
      font-weight: 400;
    }
    h4 {
      border-bottom: 1px solid var(--input-border-color);
      padding-bottom: 10px;
      margin-top: 30px;
    }
    .container { 
      max-width: 600px; 
      margin: auto; 
      background-color: var(--card-bg-color);
      padding: 20px;
      border-radius: 8px;
      box-shadow: 0 4px 8px rgba(0,0,0,0.2);
    }
    .input-group { 
      margin-bottom: 15px; 
      display: flex; 
      align-items: center; 
      justify-content: center; 
      flex-wrap: wrap;
    }
    .input-group label { 
      font-weight: bold; 
      margin-right: 10px; 
      width: 150px; 
      text-align: right; 
    }
    input[type="text"], input[type="number"], select { 
      padding: 10px; 
      border-radius: 5px; 
      border: 1px solid var(--input-border-color); 
      background-color: var(--input-bg-color);
      color: var(--text-color);
      box-shadow: inset 0 1px 3px rgba(0,0,0,0.2);
    }
    .full-width { width: 250px; }
    .dtc-group select { width: 65px; margin-right: 5px; }
    .dtc-group input { width: 100px; margin-right: 5px; }
    .dtc-group label { width: 100px; }
    .button, input[type="submit"] { 
      border: none; 
      color: white; 
      background-color: var(--primary-color); 
      padding: 10px 20px; 
      font-size: 16px; 
      cursor: pointer; 
      border-radius: 5px; 
      transition: background-color 0.3s;
    }
    .button:hover, input[type="submit"]:hover { background-color: var(--primary-color-hover); }
    #addDtc { margin-left: 5px; }
    #dtc_list_container {
      text-align:left; 
      width:270px; 
      min-height: 50px;
      background: var(--input-bg-color);
      padding: 10px;
      border-radius: 5px;
    }
    #dtc_list li {
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding: 6px 10px;
      background: #465f79;
      border-radius: 4px;
      margin-bottom: 6px;
    }
    .remove-dtc-btn {
      background-color: var(--danger-color);
      color: white;
      border: none;
      padding: 4px 8px;
      font-size: 12px;
      border-radius: 3px;
      cursor: pointer;
    }
    #status-message {
      position: fixed;
      top: 20px;
      right: 20px;
      padding: 15px 25px;
      border-radius: 5px;
      background-color: var(--primary-color);
      color: white;
      font-size: 16px;
      z-index: 1000;
      opacity: 0;
      transition: opacity 0.5s;
    }
  </style>
</head>
<body>
  <div class="container">
    <h2>OBD-II Emulator Control</h2>
    <form id="emulatorForm" action="/update">
      <div class="input-group">
        <label for="vin">VIN:</label>
        <input type="text" id="vin" name="vin" maxlength="17" class="full-width">
      </div>
      
      <h4>Diagnostic Trouble Codes (DTCs)</h4>

      <div class="input-group dtc-group">
        <label for="dtc_sys">Add DTC:</label>
        <select id="dtc_sys">
          <option value="P">P</option><option value="C">C</option><option value="B">B</option><option value="U">U</option>
        </select>
        <select id="dtc_type">
          <option value="0">0</option><option value="1">1</option><option value="2">2</option><option value="3">3</option>
        </select>
        <input type="number" id="dtc_num" min="0" max="999" class="dtc-num" placeholder="123">
        <button type="button" id="addDtc" class="button">Add</button>
      </div>

      <div class="input-group">
        <label>Active DTCs:</label>
        <div id="dtc_list_container">
          <ul id="dtc_list" style="list-style:none; padding-left:0; margin:0;"></ul>
        </div>
      </div>

      <input type="hidden" id="dtc_list_input" name="dtc_list" value="">

      <h4>Live Data</h4>
      <div class="input-group">
        <label for="temp">Engine Temp (°C):</label>
        <input type="number" id="temp" name="temp" value="90" class="full-width">
      </div>
      <div class="input-group">
        <label for="rpm">Engine RPM:</label>
        <input type="number" id="rpm" name="rpm" value="1500" class="full-width">
      </div>
      <input type="submit" value="Update Emulator">
    </form>
  </div>
  <div id="status-message"></div>

  <script>
    (function(){
      const addBtn = document.getElementById('addDtc');
      const sysEl = document.getElementById('dtc_sys');
      const typeEl = document.getElementById('dtc_type');
      const numEl = document.getElementById('dtc_num');
      const listEl = document.getElementById('dtc_list');
      const hiddenInput = document.getElementById('dtc_list_input');
      const form = document.getElementById('emulatorForm');
      const statusMessageEl = document.getElementById('status-message');
      let codes = [];

      function formatNum(v){
        let numVal = v.trim();
        if(!numVal) return "000";
        return numVal.padStart(3,'0');
      }

      function redraw(){
        listEl.innerHTML = '';
        codes.forEach((c, idx)=>{
          const li = document.createElement('li');
          const span = document.createElement('span');
          span.textContent = c;
          const btn = document.createElement('button');
          btn.type = 'button';
          btn.className = 'remove-dtc-btn';
          btn.textContent = 'Remove';
          btn.onclick = ()=>{ 
            codes.splice(idx,1); 
            updateHidden(); 
            redraw(); 
          };
          li.appendChild(span);
          li.appendChild(btn);
          listEl.appendChild(li);
        });
        updateHidden();
      }

      function updateHidden(){
        hiddenInput.value = codes.join(',');
      }

      function showStatus(message) {
        statusMessageEl.textContent = message;
        statusMessageEl.style.opacity = '1';
        setTimeout(() => {
          statusMessageEl.style.opacity = '0';
        }, 3000);
      }

      addBtn.addEventListener('click', ()=>{
        const s = sysEl.value;
        const t = typeEl.value;
        const n = formatNum(numEl.value);
        const code = s + t + n;
        if(code.length === 5 && !codes.includes(code) && codes.length < 5){
          codes.push(code);
          redraw();
          numEl.value = '';
        } else if (codes.length >= 5) {
            alert("Maximum of 5 DTCs are allowed.");
        }
      });

      if(form){
        form.addEventListener('submit', (e)=>{
          e.preventDefault();
          updateHidden();
          
          const formData = new FormData(form);
          const params = new URLSearchParams();
          for(const pair of formData) {
            if(pair[1]){ // Only add params that have a value
               params.append(pair[0], pair[1]);
            }
          }
          
          const url = form.action + '?' + params.toString();
          
          fetch(url)
            .then(response => response.text())
            .then(text => {
              console.log(text);
              showStatus(text);
            })
            .catch(error => {
              console.error('Error:', error);
              showStatus('Error updating emulator.');
            });
        });
      }
    })();
  </script>
</body>
</html>
)rawliteral";;

#endif
