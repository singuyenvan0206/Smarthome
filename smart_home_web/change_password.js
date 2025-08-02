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

function changePassword() {
  const adminPass = document.getElementById("adminPass").value.trim();
  const oldPass = document.getElementById("oldPass").value.trim();
  const newPass = document.getElementById("newPass").value.trim();
  const confirmPass = document.getElementById("confirmPass").value.trim();
  const msg = document.getElementById("passMsg");

  db.ref("config").once("value").then(snapshot => {
    const config = snapshot.val() || {};
    const currentPass = config.password;
    const adminPassword = config.admin || "admin123";

    if (adminPass !== adminPassword) {
      msg.textContent = "❌ Sai mật khẩu admin.";
    } else if (oldPass !== currentPass) {
      msg.textContent = "❌ Sai mật khẩu cũ.";
    } else if (newPass !== confirmPass) {
      msg.textContent = "❌ Mật khẩu xác nhận không khớp.";
    } else {
      db.ref("config/password").set(newPass).then(() => {
        msg.style.color = "green";
        msg.textContent = "✅ Đổi mật khẩu thành công.";
      }).catch(() => {
        msg.style.color = "red";
        msg.textContent = "❌ Lỗi khi lưu mật khẩu.";
      });
    }
  });
}
