# Entry_System
### RFID Tag controlled Entry Door Control System

Neu November 2021: RFID-Lesegerät mit Tastatur für erweiterte Sicherheit und Funktionalität

<img src="https://user-images.githubusercontent.com/42463588/139805497-821f0670-6d50-4a81-94b0-f01fccef9582.jpg" width="300"> <img src="https://user-images.githubusercontent.com/42463588/142990275-76a2b56b-06c9-4e1f-aacf-9f1de2daf6c3.jpg" width="380">

##### Prozessor Typ: Arduino Mini Pro 5 Volt
##### Components:
- Front End Controller Arduino Mini Pro<br><img src="/images/Screenshot 2018-11-06 08.17.54.png" width="300" height="" ><br> <br>
- RFID-Reader and Display Unit (mounted outside beside the entry door)<br><img src="/images/Screenshot 2018-11-06 09.19.30.png" width="200" height="" ><br>
- new from Nov/Dec 2021 on:<br>
<img src="https://user-images.githubusercontent.com/42463588/142990275-76a2b56b-06c9-4e1f-aacf-9f1de2daf6c3.jpg" width="200"><br> <br>
- 12 VDC power supply with rechargeable battery (12V 20Ah)
- Raspberry Pi 3 located in Server room - connected via serial link <br>running SYMCON Automation Software & MySQL Databse
- KESO MOZY eco (Motorzylinder) 12VDC

##### Output Signals:
- unlock KESO Motorzylinder pulse
- unlock KESO Motorzylinder permanantely
- I2C Signal to Display Unit

##### Input Signals:
- Wiegand Bus (D0 & D1) from RFID-Card reader
- KESO MOZY 'UNLOCK' signal
- 'ring' signal from Siedle Door Station
