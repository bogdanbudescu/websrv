var pressed = false;
var docHello = document.getElementById("hello")
docHello.style = "color:#c0c0c0";
document.getElementById("button").addEventListener("click", (event) => {
    (!pressed) ? docHello.style = "color:black" : docHello.style = "color:#c0c0c0";
    pressed = !pressed;
})