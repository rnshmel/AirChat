# Basic GUI interface for the AIR CHAT PCB
# written by Richard Shmel

import serial
import threading
import sys
import struct
import time
import random
from PyQt5.QtGui import QTextCursor
from PyQt5.QtCore import Qt, QTimer
from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QPushButton, QPlainTextEdit, QLineEdit,
    QGridLayout, QVBoxLayout, QWidget, QLabel, QComboBox, QTextEdit
    )

# global variables and flags
MAX_IN_MSG_LEN = 192
shutdownFlag = 0 # used to indicate program shutdown
resetFlag = 0 # used to indicate program reset
# message types for RX_ARRAY
# 0: message from a user. [0][uname][msg]   uname is in blue, msg is in black
# 1: system message. [1][C][msg]
#       C is color. 0 for green, 1 for red
RX_ARRAY = []
#MAX_RX_ARRAY_SIZE = 16
TX_ARRAY = []
CH_LIST = ["CH00","CH01","CH02","CH03","CH04","CH05","CH06","CH07",
           "CH10","CH11","CH12","CH13","CH14","CH15","CH16","CH17"]
USERNAME = ""
CHAN = 0

class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()

        self.setWindowTitle("Air Chat")

        self.configButton = QPushButton("config")
        self.configButton.setEnabled(False)
        self.configButton.clicked.connect(self.configButtonEvent)

        self.sendButton = QPushButton("send")
        self.sendButton.setEnabled(False)
        self.sendButton.clicked.connect(self.sendButtonEvent)

        self.chSelect = QComboBox()
        self.chSelect.addItems(["Channel 0-0", "Channel 0-1", "Channel 0-2","Channel 0-3", "Channel 0-4", "Channel 0-5", "Channel 0-6", "Channel 0-7",
                                "Channel 1-0", "Channel 1-1", "Channel 1-2","Channel 1-3", "Channel 1-4", "Channel 1-5", "Channel 1-6", "Channel 1-7"])

        snLabel = QLabel("username (3-16 characters)")
        crLabel = QLabel("chatroom/talkgroup channel")
        tLable = QLabel("transmit text input (1-192 characters)")

        aLabel = QLabel("AIR CHAT INTERFACE")
        aLabelFont = aLabel.font()
        aLabelFont.setPointSize(48)
        aLabel.setFont(aLabelFont)
        aLabel.setAlignment(Qt.AlignCenter)

        self.snInputBox = QLineEdit(parent=self)
        self.snInputBox.setMaxLength(16)
        self.snInputBox.textChanged.connect(self.snChangedEvent)

        self.msgOutputBox = QTextEdit(parent=self)
        self.msgOutputBox.setReadOnly(True)
        #self.msgOutputBox.cursorPositionChanged.connect(self.cursorChangedEvent)
        
        self.msgInputBox = QPlainTextEdit(parent=self)
        self.msgInputBox.textChanged.connect(self.msgChangedEvent)

        self.displayTimer=QTimer()
        self.displayTimer.timeout.connect(self.updateDisplay)
        #self.displayTimer.start()

        controlLayout = QVBoxLayout()
        controlLayout.addWidget(snLabel)
        controlLayout.addWidget(self.snInputBox)
        controlLayout.addWidget(crLabel)
        controlLayout.addWidget(self.chSelect)
        controlLayout.addWidget(self.configButton)

        sendLayout = QGridLayout()
        sendLayout.addWidget(tLable,0,0)
        sendLayout.addWidget(self.sendButton,0,1)

        layout = QGridLayout()
        layout.addLayout(controlLayout,0,0)
        layout.addWidget(aLabel,0,1)
        layout.addWidget(self.msgOutputBox,1,1)
        layout.addWidget(self.msgInputBox,2,1)
        layout.addLayout(sendLayout,3,1)
        layout.setColumnStretch(1, 3)
        layout.setRowStretch(0, 0)
        layout.setRowStretch(1, 4)
        layout.setRowStretch(2, 1)
        layout.setRowStretch(3, 0)

        widget = QWidget()
        widget.setLayout(layout)

        self.setCentralWidget(widget)

        self.displayTimer.start(100)

    def sendButtonEvent(self):
        if (len(USERNAME) >= 3):
            try:
                text = bytes(self.msgInputBox.toPlainText(), 'ASCII')
                header = struct.pack("2B",2,len(USERNAME))
                data = header + bytes(USERNAME, 'ASCII') + text + bytes(b'\xff')
                addToRxArray_USR(0,USERNAME,str(text, encoding="ASCII").strip())
                TX_ARRAY.append(data)
            except:
                addToRxArray_SYS(1, "error with text input - msg not sent")
        else:
            addToRxArray_SYS(1, "ERROR - MSG NOT SENT: invalid username string, please set a valid username")
        self.msgInputBox.clear()
        self.msgInputBox.setFocus()

    def configButtonEvent(self):
        global USERNAME
        global CHAN
        text = self.snInputBox.text()
        chan = self.chSelect.currentIndex()
        self.msgOutputBox.clear()
        msgStr = "Configuration settings: username - "+text+" | channel - "+CH_LIST[chan]
        USERNAME = text
        CHAN = int(chan)
        addToRxArray_SYS(0,msgStr)
        rnum = random.randint(10,50)
        data = struct.pack("3B",1,CHAN,rnum) + bytes(b'\xff')
        TX_ARRAY.append(data)
        
    def msgChangedEvent(self):
        text = self.msgInputBox.toPlainText()
        tLen = len(text)
        if (tLen > MAX_IN_MSG_LEN):
            text = text[:MAX_IN_MSG_LEN]
            self.msgInputBox.clear()
            self.msgInputBox.insertPlainText(text)
        if (tLen > 0):
            if (text[-1] == "\n"):
                text = text[:tLen-1]
                self.msgInputBox.clear()
                self.msgInputBox.insertPlainText(text)
            self.sendButton.setEnabled(True)
        else:
            self.sendButton.setEnabled(False)
    
    def snChangedEvent(self):
        text = self.snInputBox.text()
        tLen = len(text)
        if (tLen > 0):
            if (text[-1] == " "):
                text = text[:tLen-1]
                self.snInputBox.clear()
                self.snInputBox.setText(text)
        if (tLen >= 3 ):
            self.configButton.setEnabled(True)
        else:
            self.configButton.setEnabled(False)
    
    def updateDisplay(self):
        if (len(RX_ARRAY) > 0):
            data = RX_ARRAY.pop(0)
            print(data)
            type = data[0]
            if (type == 0):
                # user message
                # uname = blue or cyan
                # text = black
                if (data[3] == 0):
                    self.msgOutputBox.setTextColor(Qt.magenta)
                else:
                    self.msgOutputBox.setTextColor(Qt.blue)
                self.msgOutputBox.insertPlainText(data[1]+": ")
                self.msgOutputBox.setTextColor(Qt.black)
                self.msgOutputBox.insertPlainText(data[2])
            elif (type == 1):
                # system message
                # color: 0 = green, 1 = red
                if (data[1] == 0):
                    self.msgOutputBox.setTextColor(Qt.darkGreen)
                elif (data[1] == 1):
                    self.msgOutputBox.setTextColor(Qt.red)
                self.msgOutputBox.insertPlainText(data[2])
            self.msgOutputBox.insertPlainText("\n")
            self.msgOutputBox.moveCursor(QTextCursor.End, QTextCursor.MoveAnchor)

def addToRxArray_USR(color, username, message):
    data = [0,username,message,int(color)]
    RX_ARRAY.append(data)

def addToRxArray_SYS(color, message):
    data = [1,int(color),message]
    RX_ARRAY.append(data)

def txHandler(serPort):
    while True:
        if (shutdownFlag == 1):
            print("TX handler shutting down")
            break
        if (len(TX_ARRAY) > 0):
            data = TX_ARRAY.pop(0)
            print("--- DEBUG UART TX ---")
            print(data)
            serPort.write(data)
            time.sleep(3)
        time.sleep(.1)
    sys.exit()

def rxHandler(serPort, GUIwindow):
    serPort.reset_input_buffer()
    serPort.reset_output_buffer()
    while True:
        data = serPort.read_until(terminator=b'\xff',size=256)
        if (len(data) > 0):
            if data[0] == 2:
                try:
                    usernameLen = data[1]
                    username = str(data[2:usernameLen+2],'ASCII')
                    text = str(data[usernameLen+2:-1],'ASCII')
                    print("--- DEBUG UART RX ---")
                    print("data (str): " + text)
                    print("data (hex): " + data.hex())
                    addToRxArray_USR(1,username, text.strip())
                except:
                    addToRxArray_SYS(1, "Message error, unable to decode RX")
            else:
                addToRxArray_SYS(1, "PCB reset - please reconfigure username and channel")
                USERNAME = ""
                GUIwindow.snInputBox.clear()
        if (shutdownFlag == 1):
            print("RX handler shutting down")
            break
    sys.exit()

def main():
    global shutdownFlag
    print("staring program")
    # init the GUI application and the window
    GUIapp = QApplication([])
    GUIwindow = MainWindow()
    
    # serial port setup
    #comSock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    #comSock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR,1)
    #comSock.setblocking(0)
    #comSock.bind((HOST, PORT))
    print("attempting to connect to serial port: "+sys.argv[1])
    serPort = serial.Serial()
    serPort.baudrate = 1200
    serPort.port = sys.argv[1]
    try:
        serPort.open()
        print("serial port connected")
    #serPort.bytesize = serial.EIGHTBITS
    #serPort.parity = serial.PARITY_NONE
    except:
        print("unable to connect to serial port - exiting")
        sys.exit()

    # start the RX and TX threads
    rx = threading.Thread(target = rxHandler, args = (serPort,GUIwindow))
    rx.start()
    tx = threading.Thread(target = txHandler, args = (serPort,))
    tx.start()
    print("threads started")

    # last thing we do is start the gui
    GUIwindow.show()
    GUIapp.exec()
    # when the window is closed
    print("exiting program")
    shutdownFlag = 1
    serPort.cancel_read()
    serPort.cancel_write()
    serPort.close()
    rx.join()
    tx.join()
    print("threads joined")
    print("DONE")


if __name__ == "__main__":
    sys.exit(main())