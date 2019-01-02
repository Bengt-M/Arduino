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
	document.getElementById('tmpCur').innerHTML = jsonObject.t;
	document.getElementById('tmpMin').innerHTML = jsonObject.tmn;
	document.getElementById('tmpMax').innerHTML = jsonObject.tmx;
	document.getElementById('humCur').innerHTML = jsonObject.h;
	document.getElementById('humMin').innerHTML = jsonObject.hmn;
	document.getElementById('humMax').innerHTML = jsonObject.hmx;
	btn1Enable = true;
	document.getElementById('btn1').style.backgroundColor = '#00878F';
	document.getElementById('btn1').innerHTML = 'Reset';
};
connection.onclose = function(){
	document.getElementById('btn1').innerHTML = 'XXX';
	document.getElementById('btn1').style.backgroundColor = '#222';
	console.log('WebSocket connection closed');
};
function onBtn1(){
	connection.send('R');
}
