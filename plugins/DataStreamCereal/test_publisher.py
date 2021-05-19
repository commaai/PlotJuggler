#!/usr/bin/python

# todo: ensure PYTHONPATH is your cloned openpilot dir5
from cereal.messaging import PubMaster, new_message
import math
import random
from time import sleep

pm = PubMaster(['deviceState', 'carState'])

rate = 20.
time = 0.0
last_temp = 30.
v_ego = 30.

while True:
  sleep(1 / rate)
  time += 1 / rate

  msg = new_message('deviceState')
  last_temp += random.uniform(-1 / rate, 1 / rate * 2)
  msg.deviceState.batteryTempC = last_temp
  print(f'Publishing: {msg}')
  pm.send('deviceState', msg)

  msg = new_message('carState')
  v_ego += random.uniform(-1 / rate * 2, 1 / rate)
  msg.carState.vEgo = v_ego
  print(f'Publishing: {msg}')
  pm.send('carState', msg)
