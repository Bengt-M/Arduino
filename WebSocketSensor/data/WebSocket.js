var btn1Enable = false;
var connection = new WebSocket('ws://'+location.hostname+':81/', ['arduino']);
connection.onopen = function () {
	connection.send('Connect ' + new Date());
};
connection.onerror = function (error) {
	console.log('WebSocket Error ', error);
};
connection.onmessage = function (e) {  
	console.log('Server: ', e.data);
	console.log(JSON.parse(e.data));
	var jsonObject = JSON.parse(event.data);
	var t = (jsonObject.t).toFixed(1);
	var h = (jsonObject.h).toFixed(0);
	var p = jsonObject.p;
	document.getElementById('tmpCur').innerHTML = t;
	document.getElementById('humCur').innerHTML = h;
	if (p) {
		btn1Enable = true;
		document.getElementById('btn1').style.backgroundColor = '#00878F';
		document.getElementById('btn1').innerHTML = 'ON';
	} else {
		btn1Enable = false;
		document.getElementById('btn1').style.backgroundColor = '#999';
		document.getElementById('btn1').innerHTML = 'OFF';
	}
};
connection.onclose = function(){
	document.getElementById('btn1').innerHTML = 'XXX';
	document.getElementById('btn1').style.backgroundColor = '#222';
	console.log('WebSocket connection closed');
};
function onBtn1(){
	btn1Enable = !btn1Enable;
	document.getElementById('btn1').innerHTML = 'XXX';
	document.getElementById('btn1').style.backgroundColor = '#222';
	if(btn1Enable){
		connection.send('1');
	} else {
		connection.send('0');
	}  
}
