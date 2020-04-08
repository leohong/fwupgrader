# MCU Serial FW upgrader

## What is this?
This is a serial interface MCU FW upgrader create by QT.
It will parser the config.json file to know the CLI command and the baud rate settings of each customer.

## How to use the config.json
1. follow the json file struct to create your own company tag.
```
"CompanyName":                <-- Company Name
[
      "1",                    <-- Company ID
	"115200",                   <-- Baud Rate
	"set fwupgrade 1\r\n",      <-- Jump to Bootloader CLI
	"sys read_revision\r\n"     <-- Read FW version CLI
],
```
