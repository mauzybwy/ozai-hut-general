import requests
import datetime

root_url="http://192.168.1.69:6969/"

data = {
    "led" : "off",
    "relay_1" : "ON",
    "relay_2" : "ON",
    "relay_3" : "ON",
    "relay_4" : "OFF",
}

relay_data = {
    '1' : "ON",
    '2' : "ON",
    '3' : "ON",
    '4' : "ON",
}

day_data = {
    '1' : "OFF",
    '2' : "OFF",
    '3' : "ON",
    '4' : "ON",
    'time_hr' : "8",
    'time_mn' : "8",
}

night_data = {
    '1' : "ON",
    '2' : "ON",
    '3' : "OFF",
    '4' : "OFF",
    'time_hr' : "21",
    'time_mn' : "30",
}

now = datetime.datetime.now()

time_data = {
    'year' : now.year,
    'month' : now.month,
    'day' : now.day,
    'hour' : now.hour,
    'minute' : now.minute,
    'second' : now.second,
}

url = root_url + "DATETIME"
# r = requests.post(url=url, data=time_data)
r = requests.get(url=url)
print r.content

url = root_url + "DAYTIME"
# r = requests.post(url=url, data=day_data)
r = requests.get(url=url)
print r.content

url = root_url + "NIGHTTIME"
# r = requests.post(url=url, data=night_data)
r = requests.get(url=url)
print r.content

url = root_url + "RELAY"
r = requests.get(url=url)
print r.content

# url = root_url + "TIME"
# r = requests.get(url=url)
# print r.content
