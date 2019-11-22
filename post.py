import requests

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
    '1' : "ON",
    '2' : "ON",
    '3' : "ON",
    '4' : "ON",
    'time' : "",
}

night_data = {
    '1' : "ON",
    '2' : "ON",
    '3' : "ON",
    '4' : "ON",
    'time' : "",
}

# url = root_url + "RELAY"
# r = requests.post(url=url, data=relay_data)

url = root_url + "RELAY"
r = requests.get(url=url)
print r.content

url = root_url + "DAYTIME"
r = requests.get(url=url)
print r.content

# url = root_url + "TIME"
# r = requests.get(url=url)
# print r.content
