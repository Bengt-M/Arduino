#!/opt/bin/python3

# This scrpt is run on my routers lighttp server
# The NodeMCU calls this script to send sensor data
# The idea is to preserve flash life by not writing anything in the file system (anywhere) and instead send it to the web
# Info from :https://wp.josh.com/2014/06/04/using-google-spreadsheets-for-logging-sensor-data/

#import logging
import cgi
#import cgitb
import requests

#cgitb.enable(display=0, logdir="/opt/share/www/webcam", context=5, format="text")
#logging.basicConfig(filename='/opt/share/www/webcam/cgilogger.txt',level=logging.DEBUG)

#XXX comes from Google
URL = "https://script.google.com/macros/s/XXX/exec"

# Create instance of FieldStorage 
form = cgi.FieldStorage() 
# Get data from fields
if form.getvalue('time'):
   time = form.getvalue('time')
else:
   time = "Not set"
if form.getvalue('t'):
   t = form.getvalue('t')
else:
   t = "Not set"
if form.getvalue('h'):
   h = form.getvalue('h')
else:
   h = "Not set"
if form.getvalue('vcc'):
   vcc = form.getvalue('vcc')
else:
   vcc = "Not set"

# defining a params dict for the parameters to be sent to the API 
PARAMS = {'Temperature':t, 'Humidity':h, 'Time':time, 'vcc':vcc} 
# sending get request and saving the response as response object 
r = requests.get(url = URL, params = PARAMS) 
print(r)
