// User service UUID: Change this to your generated service UUID
const USER_SERVICE_UUID         = '5f961675-3050-4cef-8a98-2e7f58fb4e43'; // LED, Button
// User service characteristics
const LED_CHARACTERISTIC_UUID   = 'E9062E71-9E62-4BC6-B0D3-35CDCD9B027B';
const BTN_CHARACTERISTIC_UUID   = '62FBD229-6EDD-4D1A-B554-5C4E1BB29169';

// PSDI Service UUID: Fixed value for Developer Trial
const PSDI_SERVICE_UUID         = 'E625601E-9E55-4597-A598-76018A0D293D'; // Device ID
const PSDI_CHARACTERISTIC_UUID  = '26E2B12B-85F0-4F3F-9FDD-91D114270E6E';

// UI settings
let ledState = false; // true: LED on, false: LED off
let clickCount = 0;
let done = 0;
// -------------- //
// On window load //
// -------------- //



window.onload = () => {
    initializeApp();
};

// ----------------- //
// Handler functions //
// ----------------- //

function handlerToggleLed() {
    ledState = !ledState;

    uiToggleLedButton(ledState);
    liffToggleDeviceLedState(ledState);
}

// ------------ //
// UI functions //
// ------------ //

function uiToggleLedButton(state) {
    const el = document.getElementById("btn-led-toggle");
    el.innerText = state ? "STOP" : "START";

    if (state) {
      el.classList.add("led-on");
    } else {
      el.classList.remove("led-on");
    }
}

function uiCountPressButton() {
    clickCount++;

    const el = document.getElementById("click-count");
    el.innerText = clickCount;
}

function uiToggleStateButton(pressed) {
    const el = document.getElementById("btn-state");

    if (pressed) {
        el.classList.add("pressed");
        el.innerText = "ON";
    } else {
        el.classList.remove("pressed");
        el.innerText = "OFF";
    }
}

function uiToggleDeviceConnected(connected) {
    const elStatus = document.getElementById("status");
    const elControls = document.getElementById("controls");

    elStatus.classList.remove("error");

    if (connected) {
        // Hide loading animation
        uiToggleLoadingAnimation(false);
        // Show status connected
        elStatus.classList.remove("inactive");
        elStatus.classList.add("success");
        elStatus.innerText = "Device connected";
        // Show controls
        elControls.classList.remove("hidden");
    } else {
        // Show loading animation
        uiToggleLoadingAnimation(true);
        // Show status disconnected
        elStatus.classList.remove("success");
        elStatus.classList.add("inactive");
        elStatus.innerText = "Device disconnected";
        // Hide controls
        elControls.classList.add("hidden");
    }
}

function uiToggleLoadingAnimation(isLoading) {
    const elLoading = document.getElementById("loading-animation");

    if (isLoading) {
        // Show loading animation
        elLoading.classList.remove("hidden");
    } else {
        // Hide loading animation
        elLoading.classList.add("hidden");
    }
}

function uiStatusError(message, showLoadingAnimation) {
    uiToggleLoadingAnimation(showLoadingAnimation);

    const elStatus = document.getElementById("status");
    const elControls = document.getElementById("controls");

    // Show status error
    elStatus.classList.remove("success");
    elStatus.classList.remove("inactive");
    elStatus.classList.add("error");
    elStatus.innerText = message;

    // Hide controls
    elControls.classList.add("hidden");
}

function makeErrorMsg(errorObj) {
    return "Error\n" + errorObj.code + "\n" + errorObj.message;
}

// -------------- //
// LIFF functions //
// -------------- //

function initializeApp() {
    liff.init(() => initializeLiff("1610743821-l6D0pnmR"), error => uiStatusError(makeErrorMsg(error), false));
}

function initializeLiff(myLiffId) {
    liff
        .init({
            liffId: myLiffId
        })
        .then(() => {
            // start to use LIFF's api
            //initializeApp();
            
            liff.initPlugins(['bluetooth']).then(() => {
                liffCheckAvailablityAndDo(() => liffRequestDevice());
            }).catch(error => {
                uiStatusError(makeErrorMsg(error), false);
            });
            
        })
        .catch((err) => {
            document.getElementById("liffAppContent").classList.add('hidden');
            document.getElementById("liffInitErrorMessage").classList.remove('hidden');
        });
}

/*
function initializeLiff() {
    liff.initPlugins(['bluetooth']).then(() => {
        
        liffCheckAvailablityAndDo(() => liffRequestDevice());
    }).catch(error => {
        uiStatusError(makeErrorMsg(error), false);
    });
}
*/

function liffCheckAvailablityAndDo(callbackIfAvailable) {
    // Check Bluetooth availability
    liff.bluetooth.getAvailability().then(isAvailable => {
        if (isAvailable) {
            uiToggleDeviceConnected(false);
            callbackIfAvailable();
        } else {
            uiStatusError("Bluetooth not available", true);
            setTimeout(() => liffCheckAvailablityAndDo(callbackIfAvailable), 10000);
        }
    }).catch(error => {
        uiStatusError(makeErrorMsg(error), false);
    });;
}

function liffRequestDevice() {
    liff.bluetooth.requestDevice().then(device => {
        liffConnectToDevice(device);
    }).catch(error => {
        uiStatusError(makeErrorMsg(error), false);
    });
}

function liffConnectToDevice(device) {
    device.gatt.connect().then(() => {
        //document.getElementById("device-name").innerText = device.name;
        //document.getElementById("device-id").innerText = device.id;

        // Show status connected
        uiToggleDeviceConnected(true);

        // Get service
        device.gatt.getPrimaryService(USER_SERVICE_UUID).then(service => {
            liffGetUserService(service);
        }).catch(error => {
            uiStatusError(makeErrorMsg(error), false);
        });
        device.gatt.getPrimaryService(PSDI_SERVICE_UUID).then(service => {
            liffGetPSDIService(service);
        }).catch(error => {
            uiStatusError(makeErrorMsg(error), false);
        });

        // Device disconnect callback
        const disconnectCallback = () => {
            // Show status disconnected
            uiToggleDeviceConnected(false);

            // Remove disconnect callback
            device.removeEventListener('gattserverdisconnected', disconnectCallback);

            // Reset LED state
            ledState = false;
            // Reset UI elements
            uiToggleLedButton(false);
            uiToggleStateButton(false);

            // Try to reconnect
            initializeLiff();
        };

        device.addEventListener('gattserverdisconnected', disconnectCallback);
    }).catch(error => {
        uiStatusError(makeErrorMsg(error), false);
    });
}

function liffGetUserService(service) {
    // Button pressed state
    service.getCharacteristic(BTN_CHARACTERISTIC_UUID).then(characteristic => {
        liffGetButtonStateCharacteristic(characteristic);
    }).catch(error => {
        uiStatusError(makeErrorMsg(error), false);
    });

    // Toggle LED
    service.getCharacteristic(LED_CHARACTERISTIC_UUID).then(characteristic => {
        window.ledCharacteristic = characteristic;

        // Switch off by default
        //liffToggleDeviceLedState(false);
        
    }).catch(error => {
        uiStatusError(makeErrorMsg(error), false);
    });
}

function liffGetPSDIService(service) {
    // Get PSDI value
    service.getCharacteristic(PSDI_CHARACTERISTIC_UUID).then(characteristic => {
        return characteristic.readValue();
    }).then(value => {
        // Byte array to hex string
        const psdi = new Uint8Array(value.buffer)
            .reduce((output, byte) => output + ("0" + byte.toString(16)).slice(-2), "");
        //document.getElementById("device-psdi").innerText = psdi;
    }).catch(error => {
        uiStatusError(makeErrorMsg(error), false);
    });
}

function liffGetButtonStateCharacteristic(characteristic) {
    // Add notification hook for button state
    // (Get notified when button state changes)
    characteristic.startNotifications().then(() => {
        characteristic.addEventListener('characteristicvaluechanged', e => { 
            const temperature = document.getElementById("temperature");
            const timeout = document.getElementById("timeout");
            const mode = document.getElementById("mode");
            const ltime = document.getElementById("ltime");
            const rtime = document.getElementById("rtime");
            const stime = document.getElementById("stime");
            const nshake = document.getElementById("nshake");
            const stemp = document.getElementById("stemp");

            const val = (new Uint8Array(e.target.value.buffer));
            var value = String.fromCharCode.apply(null, val);
            var vals = value.split(',');
            
            temperature.innerText = vals[0]+'C';
            timeout.innerText = vals[1];
            var relaystatus = vals[2];
            var m = vals[3];
            var timeouts = parseInt(vals[1]);
            if (relaystatus == '1' ){
                uiToggleStateButton(true);  
            }
            else {
                uiToggleStateButton(false);
            }
            if (timeouts < 1) {
                uiToggleLedButton(false);
            }
            else {
                uiToggleLedButton(true);
            }
            if (m == 1) {
                mode.innerText = 'Manual';
            }
            else {
                mode.innerText = 'Auto';
            }
            //3, r_time, l_time, s_time, n_shake,set_temp
            if (done != 1) {
                rtime.value = vals[4];
                ltime.value = vals[5];
                stime.value = vals[6];
                nshake.value = vals[7];
                stemp.value = vals[8];
                done = 1;
            }
        });
    }).catch(error => {
        uiStatusError(makeErrorMsg(error), false);
    });
}

function liffToggleDeviceLedState(state) {
    // on: 0x01
    // off: 0x00
    var ltime = document.getElementById("ltime").value;
    var rtime = document.getElementById("rtime").value;
    var stime = document.getElementById("stime").value;
    var nshake = document.getElementById("nshake").value;
    var stemp = document.getElementById("stemp").value;
    var cmd='0';
    
    if (state) {
        uiToggleLedButton(true);
        cmd = '1';
    }
    else {
        uiToggleLedButton(false);
        cmd='0';
    }
    //3, r_time, l_time, s_time, n_shake,set_temp
    /*
     // retrieve configuration for LIFF and set global variables
    String ls = getValue(str, ',', 0);   // left rotate
    l_time = ls.toInt();
    String rs = getValue(str, ',', 1);   // right rotate
    r_time = rs.toInt();
    String ss = getValue(str, ',', 2);   // stop time
    s_time = ss.toInt();
    String ns = getValue(str, ',', 4);   // no shake
    n_shake = ns.toInt();
    String ts = getValue(str, ',', 5);   // set temp
    set_temp = ts.toFloat();
    String cs = getValue(str, ',', 6);   // command start/stop from LIFF
    cmd = cs.toInt();
    */    
    //var msg = stime+","+tlow+","+thigh+","+cmd
    var msg = ltime+","+rtime+","+stime+","+nshake+","+stemp+","+cmd
    
    window.ledCharacteristic.writeValue(new TextEncoder().encode(msg))
    .catch(error => {
        uiStatusError(makeErrorMsg(error), false);
    });
}
