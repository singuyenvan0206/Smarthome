const firebaseConfig = {
  apiKey: "AIzaSyCupFMgpx4fzkG79wfRw4C6cqE1OgHa-ZY",
  authDomain: "smarthome-3b496.firebaseapp.com",
  databaseURL: "https://smarthome-3b496-default-rtdb.firebaseio.com",
  projectId: "smarthome-3b496",
  storageBucket: "smarthome-3b496.appspot.com",
  messagingSenderId: "383372706017",
  appId: "1:383372706017:web:2975c26eab51c77fc1f7a0",
  measurementId: "G-X7VV1GZBTE"
};

firebase.initializeApp(firebaseConfig);
const db = firebase.database();

let ledState = false;
let servoState = {servo1: false, servo2: false};
let autoServo2 = false;
let currentPage = 1;
const itemsPerPage = 10;
let logEntries = [];


function updateLabels(){
  document.getElementById("ledBtn").innerText = ledState ? "Tắt đèn" : "Bật đèn";
  document.getElementById("servo1Btn").innerText = servoState.servo1 ? "Đóng cửa" : "Mở cửa";
  document.getElementById("servo2Btn").innerText = servoState.servo2 ? "Tắt quạt" : "Bật quạt";
  document.getElementById("autoBtn").innerText = "Tự động bật quạt: " + (autoServo2 ? "BẬT" : "TẮT");
}

db.ref("/control").on("value", snap => {
  const d = snap.val() || {};
  ledState = d.led === "ON";
  servoState.servo1 = d.servo1 === "OPEN";
  servoState.servo2 = d.servo2 === "OPEN";
  autoServo2 = !!d.auto_servo2;
  updateLabels();
});

db.ref("/sensor").on("value", snap => {
  const d = snap.val() || {};
  document.getElementById("temp").innerText = d.temperature ?? "--";
  document.getElementById("humi").innerText = d.humidity ?? "--";
});

db.ref("/log/servo1").on("value", snap => {
  const body = document.querySelector("#logTable tbody");
  body.innerHTML = "";
  const entries = Object.entries(snap.val() || {}).sort((a,b) => b[0] - a[0]);
  entries.forEach(([k,v],i) => {
    const r = document.createElement("tr");
    r.innerHTML = `<td>${i+1}</td><td>${v}</td>`;
    body.appendChild(r);
  });
});
db.ref("/log/servo1").on("value", snap => {
  logEntries = Object.entries(snap.val() || {}).sort((a,b) => b[0] - a[0]);
  currentPage = 1;
  renderLogTable();
});
function nextPage() {
  if (currentPage * itemsPerPage < logEntries.length) {
    currentPage++;
    renderLogTable();
  }
}

function prevPage() {
  if (currentPage > 1) {
    currentPage--;
    renderLogTable();
  }
}

function clearLog() {
  if (confirm("Bạn có chắc chắn muốn xóa toàn bộ lịch sử mở cửa không?")) {
    db.ref("/log/servo1").remove();
  }
}
function renderLogTable() {
  const body = document.querySelector("#logTable tbody");
  body.innerHTML = "";
  const start = (currentPage - 1) * itemsPerPage;
  const entries = logEntries.slice(start, start + itemsPerPage);
  entries.forEach(([k,v],i) => {
    const r = document.createElement("tr");
    r.innerHTML = `<td>${start + i + 1}</td><td>${v}</td>`;
    body.appendChild(r);
  });
  document.getElementById("pageInfo").innerText = `Trang ${currentPage} / ${Math.ceil(logEntries.length / itemsPerPage) || 1}`;
}
function toggleLED() {
  ledState = !ledState;
  db.ref("/control/led").set(ledState ? "ON" : "OFF");
}

function toggleServo(s) {
  servoState[s] = !servoState[s];
  db.ref(`/control/${s}`).set(servoState[s] ? "OPEN" : "CLOSE");
  if (s === "servo1" && servoState[s]) {
    const n = new Date();
    const f = `${n.getDate().toString().padStart(2,'0')}/${(n.getMonth()+1).toString().padStart(2,'0')}/${n.getFullYear()} - ${n.getHours().toString().padStart(2,'0')}:${n.getMinutes().toString().padStart(2,'0')}:${n.getSeconds().toString().padStart(2,'0')}`;
    db.ref(`/log/servo1/${Date.now()}`).set(f);
  }
}

function toggleAuto() {
  autoServo2 = !autoServo2;
  db.ref("/control/auto_servo2").set(autoServo2);
}

function updateThreshold() {
  const val = parseFloat(document.getElementById("thresholdTemp").value);
  const msg = document.getElementById("thresholdMsg");

  if (isNaN(val)) {
    msg.style.color = "red";
    msg.textContent = "❌ Vui lòng nhập số hợp lệ.";
    return;
  }

  db.ref("/config/servo2_threshold").set(val)
    .then(() => {
      msg.style.color = "green";
      msg.textContent = "✅ Cập nhật thành công.";
    })
    .catch((err) => {
      msg.style.color = "red";
      msg.textContent = "❌ Lỗi cập nhật: " + err.message;
    });
}
function resetPassword() {
  const email = document.getElementById("email").value;
  if (!email) {
    document.getElementById("resetMsg").style.color = "red";
    document.getElementById("resetMsg").innerText = "Vui lòng nhập email để đặt lại mật khẩu.";
    return;
  }

  firebase.auth().sendPasswordResetEmail(email)
    .then(() => {
      document.getElementById("resetMsg").style.color = "green";
      document.getElementById("resetMsg").innerText = "Email đặt lại mật khẩu đã được gửi.";
    })
    .catch((error) => {
      document.getElementById("resetMsg").style.color = "red";
      document.getElementById("resetMsg").innerText = error.message;
    });
}
// Firebase config (đảm bảo bạn đã có firebase.initializeApp({...}))
// Đăng nhập
function login() {
  const email = document.getElementById("email").value;
  const pass = document.getElementById("password").value;
  firebase.auth().signInWithEmailAndPassword(email, pass)
    .then(() => {
      document.getElementById("loginSection").style.display = "none";
      document.getElementById("appSection").style.display = "block";
      loadAppData(); // khởi tạo dashboard
    })
    .catch(err => {
      document.getElementById("loginError").innerText = err.message;
    });
}

// Tự động nhận diện login
firebase.auth().onAuthStateChanged(user => {
  if (user) {
    document.getElementById("loginSection").style.display = "none";
    document.getElementById("appSection").style.display = "block";
    loadAppData(); // khởi tạo dashboard nếu đã đăng nhập
  }
});

// Đăng xuất
function logout() {
  firebase.auth().signOut().then(() => location.reload());
}
