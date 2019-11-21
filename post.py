import requests

url="http://192.168.1.242/RELAY/1"
r = requests.post(url=url)
r = requests.get(url=url)
print r.content

url="http://192.168.1.242/TIME"
r = requests.get(url=url)
print r.content
