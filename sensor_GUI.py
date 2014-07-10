"""
Name:        ECG v3 python code for wireless transmission and ADC conversion
Purpose:     Plots the voltages communicated through UART from ez430-RF2500
             and filters the high frequency interference
Author:      Ben Peng, University of Rochester, NY.
Updated:     Feb 8 2012
"""
#import warnings

#warning.filterwarnings("ignore", category=SyntaxWarning)

import os
import pprint
import random
import sys
import wx
import serial
import scipy
import math
import csv

# The recommended way to use wx with mpl is with the WXAgg
# backend.
#
import matplotlib
matplotlib.use('WXAgg')
from matplotlib.figure import Figure
from matplotlib.backends.backend_wxagg import \
    FigureCanvasWxAgg as FigCanvas, \
    NavigationToolbar2WxAgg as NavigationToolbar
import numpy as np
import pylab

buffersize = 4600 # data to be shown in screen
                  # 4600: 20 sec
buffer_for_DSP = 230 # must less than buffer size 1397 is the total sample one ADC can be stored in flash mem for Fs=230Hz
                     # 230: 1 sec
                     # 460: 2 sec
cutoff_pt = 40 # cutoff point 25 is arount 20 Hz
x_length_factor = 10 # enter times of buffer_for_DSP
PROC = 0
# ==================================== define DataRead =========================================
class DataRead(object): # DataRead has been defined as class
    """
    This class reads data from the MSP430 serial port
    """
    #Flag to tell if new data has been read
    drawIt = 0
    counter = 0
    readingData = 0

    #Variable arrays to keep track of data; [] is list which is albe to changed
    A0 = []
    A1 = []
    A2 = []
    A3 = []
    A4 = []
    A12 = []
    A13 = []
    A14 = []
    xValues = []

    #initialize arrays to have [buffersize] elements, will keep track of
    #the last [buffersize] data points
    for i in range(buffersize):
        A0.append(0)
        A1.append(0)
        A2.append(0)
        A3.append(0)
        A4.append(0)
        A12.append(0)
        A13.append(0)
        A14.append(0)
        xValues.append(i)

    #Global variable to keep track of serial port configuration
    ser = serial.Serial()
    ser.baudrate = 9600
    ser.timeout = .1
    port = 0

    def __init__(self): # Everytime for DataRead must go through this path. this is for the definition of CMD welcome msg
        print "\n------------------------------------------------------------------"
        print "MSP430 ADC value viewer\nPress Ctrl+C to exit at any time"
        print "------------------------------------------------------------------"
        print "Available serial ports:"
        for n,s in self.scan():
            print "(%d) %s" % (n,s)


        #Ask user which serial port to open
        while self.port == 0:
            self.port = raw_input("\nWhich serial port is connected to the MSP430?\n\
    (Enter the number in parentheses or 'skip' for no connection)")
            if self.port == "skip":
                break
            try:
                self.port = int(self.port)
            except ValueError:
                self.port = 0
                print "error: port selection must be an integer"

        #Got the serial port information, use it
        if self.port != "skip":
            self.ser.port = self.port

            try:
                self.ser.open()
            except:
                print "Could not connect to serial port %d, exiting" %self.port
                exit()

            if self.ser.isOpen() == True:
                print "Serial port " + self.ser.portstr + " successfully opened, listening for data"
                print "Press Ctrl+C to exit or select File->Exit from the menu bar\n"
        else:
            print "Skipping serial connection, no serial port will be opened"

    def startADC(self): # define what to do for startADC.
        #send the ADC start signal through the serial port
        if self.ser.isOpen() == True:
            #send the letter s
            print "Sending ADC start signal through serial port"
            self.ser.write("s")
        else:
                print "Could not send ADC start signal, serial port is not open"

    def read(self):
        #read data from the serial port
        if self.ser.isOpen() == True:
            #read a serial port until endline ("\n")
            serialmsg = self.ser.readline() 


            #if we read something, parse and convert ADC values to volts
            if len(serialmsg) > 0:
                if len(serialmsg)> 4 and serialmsg[4] == ',':
                    #parse message data
                    serialmsg = serialmsg.strip()
                    ADCvals = serialmsg.split(', ')

                    #Check for complete data set notification
                    if ADCvals[0] == '0682' \
                        and ADCvals[1] == '0682' \
                        and ADCvals[2] == '0682' \
                        and ADCvals[3] == '0682' \
                        and ADCvals[4] == '0682' \
                        and ADCvals[5] == '0682' \
                        and ADCvals[6] == '0682' \
                        and ADCvals[7] == '0682':
                        self.drawIt = 1
                        self.readingData = 0
		    #----- want to run ADC automatically -------
                        self.startADC()
		    #-------------------------------------------
                    else:
                        try:
                            for i in range (len(ADCvals)):
                                ADCvals[i] = float(ADCvals[i])*3.3/1023
                            print ["%0.3f" %i for i in ADCvals]
		            
                            self.update_data_arrays(ADCvals)
                        except ValueError:
                            pass
                else:
                    print serialmsg,
        else:
            if self.port != "skip":
                print "serial port not open"


    #Scan function from the pySerial example code
    def scan(self):
        """scan for available ports. return a list of tuples (num, name)"""
        available = []
        for i in range(256):
            try:
                s = serial.Serial(i)
                available.append( (i, s.portstr))
                s.close()   # explicit close 'cause of delayed GC in java
            except serial.SerialException:
                pass
        return available
    def closeSerial(self):
        """Closes serial port for clean exit"""
        print "Closing serial port"
        try:
            self.ser.close();
        except:
            pass

    #This function updates the data arrays when given the latest data
#    @staticmethod
#    def update_plot_lock():
#        global PROC
#        PROC = 1 
#        print "Udated completed: PROC:", PROC

    
    def update_data_arrays(self, lastReading):
        if len(lastReading) >= 8:
            self.A0.append(lastReading[0])
            self.A1.append(lastReading[1])
            self.A2.append(lastReading[2])
            self.A3.append(lastReading[3])
            self.A4.append(lastReading[4])
            self.A12.append(lastReading[5])
            self.A13.append(lastReading[6])
            self.A14.append(lastReading[7])
            self.A0.pop(0)
            self.A1.pop(0)
            self.A2.pop(0)
            self.A3.pop(0)
            self.A4.pop(0)
            self.A12.pop(0)
            self.A13.pop(0)
            self.A14.pop(0)
            self.counter += 1
            if self.counter%8 == 0: # this one control the figure flash rate! 50 is original
                self.drawIt = 1
            if self.counter%(buffer_for_DSP) == 0: # want to save data length for DSP
	            #----- want to save data automatically
                path_default = os.getcwd()+'/sample.csv'
                self.save_data_arrays_auto(path_default)
#                self.PROC = 1 # hope to enable plot the new data
                global PROC
                PROC = 1
                print "plot new processed data PROC:", PROC
#                app_rev = wx.PySimpleApp()
#                app_rev.frame_rev = GraphFrame_rev()
#                app_rev.frame_rev.draw_plot_rev(self.PROC)
#                #app_rev.frame_rev.Show()    
#                app_rev.MainLoop()



        else:
            print "Could not append corrupted data"
#            self.startADC()

    def save_data_arrays(self, path):
        #write the data to the file
        datastring = ""
        thefile = open(path, 'w')

        #Write data Labels to file
        datastring = "A0, A1, A2, A3, A4, A12, A13, A14\n"
        thefile.write(datastring)
        #Now write the data
        for i in range(buffersize):
            datastring =    "%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n" %(self.A0[i],
                    self.A1[i], self.A2[i], self.A3[i], self.A4[i], self.A12[i], self.A13[i], self.A14[i])
#            datastring =    "%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n" %(self.A14[i] - self.A12[i] - self.A1[i] + self.A3[i],
#                    self.A14[i] - self.A12[i] - self.A1[i] + self.A3[i], self.A14[i] - self.A12[i] - self.A1[i] + self.A3[i], self.A14[i] - self.A12[i] - self.A1[i] + self.A3[i], self.A14[i] - self.A12[i] - self.A1[i] + self.A3[i], self.A14[i] - self.A12[i] - self.A1[i] + self.A3[i], self.A14[i] - self.A12[i] - self.A1[i] + self.A3[i], self.A14[i] - self.A12[i] - self.A1[i] + self.A3[i])
            thefile.write(datastring)
        thefile.close()
        #print "save successful"
#-------------------save sliced data for DSP -----------------------------
    def save_data_arrays_auto(self, path):
        #write the data to the file
        datastring = ""
        thefile = open(path, 'w')
        thefile_rev = open(os.getcwd()+'/dif.csv', 'w')

        #Write data Labels to file
        datastring = "A0, A1, A2, A3, A4, A12, A13, A14\n"
        thefile.write(datastring)
        #Now write the data
        #for i in range(buffer_for_DSP): # can change for later data operation
        for i in range(buffersize-buffer_for_DSP, buffersize): # can change for later data operation
            datastring =    "%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n" %(self.A0[i],
                    self.A1[i], self.A2[i], self.A3[i], self.A4[i], self.A12[i], self.A13[i], self.A14[i])
#            datastring =    "%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n" %(self.A14[i] - self.A12[i] - self.A1[i] + self.A3[i],
#                    self.A14[i] - self.A12[i] - self.A1[i] + self.A3[i], self.A14[i] - self.A12[i] - self.A1[i] + self.A3[i], self.A14[i] - self.A12[i] - self.A1[i] + self.A3[i], self.A14[i] - self.A12[i] - self.A1[i] + self.A3[i], self.A14[i] - self.A12[i] - self.A1[i] + self.A3[i], self.A14[i] - self.A12[i] - self.A1[i] + self.A3[i], self.A14[i] - self.A12[i] - self.A1[i] + self.A3[i])
            thefile.write(datastring)
	#----- want to get the differential signal from Electrode 2 and Electrode 3 for period depends on buffer_for_DSP
            #dif_signal = self.A1[i] - self.A12[i] #self.A3[i] - self.A12[i]
            dif_signal = self.A14[i] - self.A12[i] - self.A1[i] + self.A3[i]
			#dif_signal = self.A14[i] - self.A12[i] - self.A1[i] + self.A3[i] #self.A3[i] - self.A12[i]
            dif_string = "%.3f\n" %(dif_signal)
            thefile_rev.write(dif_string) 
	    #transpose_dif = transpose(dif_signal)
	    #print dif_signal
        thefile.close()
        thefile_rev.close()
        print "save diff successfully"
	#----- want to open dif signal and do ifft --------------------------------------

        dif_sig = csv.reader(open("dif.csv", "rU"),delimiter='\n')
        dif_sig_list = []
        dif_sig_list.extend(dif_sig)
        signal = []
        for data in dif_sig_list:
         signal.append(data[0])
         #print signal

        def convertStr(s):
         try:
          ret = int(s)
         except ValueError:
          ret = float(s)
         return ret

        t = range(len(signal))
        for i in range(len(signal)):
         t[i] = convertStr(signal[i])

        raw_fft = scipy.fft(t) # (G) and (H)  
        bp = raw_fft.copy()  
#        bp *= real(fft_raw.dot(raw_fft))/real(bp.dot(bp))
        for i in range(len(bp)): # (H-red)  
            if i>=cutoff_pt:
             bp[i]=0  


        reconstruct = abs(scipy.ifft(bp)) 
        np.savetxt('reconstruct.txt', reconstruct)
        print "processed data is ready to use"
        #self.PROC = 1
        #-----------------------------------------------------------------------
#--------------------------------------------------
## definition of 2nd class, the first one is ReadData
class GraphFrame(wx.Frame):
    """ The main frame of the application
    """
    title = 'MSP430 ADC Value Plotter'
    refreshCounter = 0
#    PROC_cue = 0

    def __init__(self):
        wx.Frame.__init__(self, None, -1, self.title)

        #Instantiate DataRead class to talk to serial port
        self.dataread = DataRead()

        #Create wx window
        self.create_menu()
        self.create_status_bar()
        self.create_main_panel()


        #Start recurring timer for updating plot
        self.redraw_timer = wx.Timer(self)
        self.Bind(wx.EVT_TIMER, self.on_redraw_timer, self.redraw_timer)
        self.redraw_timer.Start(3)

    def create_menu(self):
        #Configure window menu bar
        self.menubar = wx.MenuBar()

        menu_file = wx.Menu()
        m_expt = menu_file.Append(-1, "&Save plot image\tCtrl-S", "Save plot image")
        self.Bind(wx.EVT_MENU, self.on_save_plot, m_expt)
        m_startadc = menu_file.Append(-1, "&Start ADC BEN\tCtrl-G",\
          "Trigger ADC to start taking data")
        self.Bind(wx.EVT_MENU, self.on_startADC, m_startadc)
        menu_file.AppendSeparator()
        m_exit = menu_file.Append(-1, "&Exit\tCtrl-X", "Exit")
        self.Bind(wx.EVT_MENU, self.on_exit, m_exit)    #exit event handler
        self.Bind(wx.EVT_CLOSE, self.on_close)      #window close event handler

        self.menubar.Append(menu_file, "&File")
        self.SetMenuBar(self.menubar)

    def create_main_panel(self): 
        #Configure main window panel THIS realtime signal frame
        self.panel = wx.Panel(self)

        self.init_plot()
        self.canvas = FigCanvas(self.panel, -1, self.fig)

        self.save_button = wx.Button(self.panel, -1, "Save plot data")
        self.Bind(wx.EVT_BUTTON, self.on_save_button, self.save_button)
        self.startADC_button= wx.Button(self.panel, -1, "    Initialize ADC    ")
        self.Bind(wx.EVT_BUTTON, self.on_startADC, self.startADC_button)

        self.hbox1 = wx.BoxSizer(wx.HORIZONTAL)
        self.hbox1.Add(self.save_button, border=5, flag=wx.ALL | wx.ALIGN_CENTER_VERTICAL )
        self.hbox1.Add(self.startADC_button, border=5, flag=wx.ALL | wx.ALIGN_CENTER_VERTICAL)

        self.vbox = wx.BoxSizer(wx.VERTICAL)
        self.vbox.Add(self.canvas, 1, flag=wx.LEFT | wx.TOP | wx.GROW)
        self.vbox.Add(self.hbox1, 0, flag=wx.ALIGN_RIGHT)

        self.panel.SetSizer(self.vbox)
        self.vbox.Fit(self)

    def create_status_bar(self):
        self.statusbar = self.CreateStatusBar()

    def init_plot(self):
        self.dpi = 100
        self.fig = Figure((5.0, 4.0), dpi=self.dpi)

        self.axes = self.fig.add_subplot(111)
        self.axes.set_axis_bgcolor('black')
        self.axes.set_title('MSP430 ADC Values', size=12)
        self.axes.grid(True, color='gray')
        self.axes.axis([0,buffersize,-1,3.6])   #Axes limits:([xmin,xmax,ymin,ymax])

        pylab.setp(self.axes.get_xticklabels(), fontsize=8)
        pylab.setp(self.axes.get_yticklabels(), fontsize=8)

        # plot the data as a line series, and save the reference
        # to the plotted line series
        self.line0, = self.axes.plot(DataRead.xValues, DataRead.A0, label="PX_OUT1")
        self.line1, = self.axes.plot(DataRead.xValues, DataRead.A1, label="ECG_OUT1")
        self.line2, = self.axes.plot(DataRead.xValues, DataRead.A2, label="PX_OUT2")
        self.line3, = self.axes.plot(DataRead.xValues, DataRead.A3, label="ECG_OUT2")
        self.line4, = self.axes.plot(DataRead.xValues, DataRead.A4, label="PX_OUT3")
        self.line12, = self.axes.plot(DataRead.xValues, DataRead.A12, label="ECG_OUT3")
        self.line13, = self.axes.plot(DataRead.xValues, DataRead.A13, label="PX_OUT4")
        self.line14, = self.axes.plot(DataRead.xValues, DataRead.A14, label="ECG_OUT4")
        self.axes.legend(loc='upper center',
            prop=matplotlib.font_manager.FontProperties(size='xx-small'),
            ncol=8)
        #

    def draw_plot(self):
        """ Redraws the plot
        """
        #Read serial data and update the plot
        self.dataread.read()
#        self.PROC_cue = self.dataread.PROC
#        print "PROC_CUE = ",self.PROC_cue

        self.line0.set_ydata(DataRead.A0)
        self.line1.set_ydata(DataRead.A1)
        self.line2.set_ydata(DataRead.A2)
        self.line3.set_ydata(DataRead.A3)
        self.line4.set_ydata(DataRead.A4)
        self.line12.set_ydata(DataRead.A12)
        self.line13.set_ydata(DataRead.A13)
        self.line14.set_ydata(DataRead.A14)

        #Only readraw canvas only if the data set is complete
        #Do this to minimize the effect of suspected memory leak in canvas.draw()
        if self.dataread.drawIt == 1:
            self.canvas.draw()
            self.dataread.drawIt = 0
            #if end of data set, re-enable "start ADC" button
            if self.dataread.readingData == 0:
                self.startADC_button.SetLabel("Start ADC")
                self.startADC_button.Enable()

    def on_startADC(self, event):
        if self.dataread.readingData == 0:
            print "start ADC was triggered"
            self.dataread.startADC()
            print "trigger was sent, waiting for data"
            #Disable the button and menu item until data acquisition is finished
            self.startADC_button.Disable()
            self.startADC_button.SetLabel("Reading data...")
            self.dataread.readingData = 1
        else:
            print "ADC operation already in progress"


    def on_save_button(self, event):
        print "save button was pressed"
        #Write the data to a file, launch a file dialog box
        file_choices = "CSV (*.csv)|*.csv"
        dlg = wx.FileDialog(
            self,
            message="Save plot data as...",
            defaultDir=os.getcwd(),
            defaultFile="MSP430data.csv",
            wildcard=file_choices,
            style=wx.SAVE | wx.OVERWRITE_PROMPT)

        if dlg.ShowModal() == wx.ID_OK: # it is passed requisit requirement to save the following format for col and rows.
            path = dlg.GetPath()
            self.dataread.save_data_arrays(path)



    def on_save_plot(self, event):
        file_choices = "PNG (*.png)|*.png"

        dlg = wx.FileDialog(
            self,
            message="Save plot as...",
            defaultDir=os.getcwd(),
            defaultFile="plot.png",
            wildcard=file_choices,
            style=wx.SAVE | wx.OVERWRITE_PROMPT)

        if dlg.ShowModal() == wx.ID_OK:
            path = dlg.GetPath()
            self.canvas.print_figure(path, dpi=self.dpi)
            self.flash_status_message("Saved to %s" % path)

    def on_redraw_timer(self, event):
        self.draw_plot()
        self.refreshCounter += 1

    def on_exit(self, event):
        self.redraw_timer.Stop()
        self.dataread.closeSerial()
        self.Destroy()

    def on_close(self, event):
        self.redraw_timer.Stop()
        self.dataread.closeSerial()
        self.Destroy()

    def flash_status_message(self, msg, flash_len_ms=1500):
        self.statusbar.SetStatusText(msg)
        self.timeroff = wx.Timer(self)
        self.Bind(
            wx.EVT_TIMER,
            self.on_flash_status_off,
            self.timeroff)
        self.timeroff.Start(flash_len_ms, oneShot=True)

    def on_flash_status_off(self, event):
        self.statusbar.SetStatusText('')
####---------- processed windows--------------------------------------------------

class GraphFrame_rev(wx.Frame):
    """ The main frame of the RECONSTRUCTED signal
    """
    title = 'Processed ECG Value Plotter'
    refreshCounter = 0
    strob = 0

    def __init__(self):
        wx.Frame.__init__(self, None, -1, self.title)

        #Instantiate DataRead class to talk to serial port
        #self.dataread = GraphFrame.save_data_arrays_auto.reconstruct
#        self.READ = DataRead.read
#        self.control = GraphFrame.PROC_cue

        #Create wx window
        self.create_sub_panel()
        self.create_status_bar()

        #Start recurring timer for updating plot
        self.redraw_timer = wx.Timer(self)
        self.Bind(wx.EVT_TIMER, self.on_redraw_timer, self.redraw_timer)
        #if DataRead.PROC == 1:
        self.redraw_timer.Start(1) # 1000 millisec for restarting the timer
#        self.redraw_timer.Start((1.000/230)*buffer_for_DSP*20000) # 1000 millisec for restarting the timer
        #DataRead.PROC = 0
      
#        self.draw_plot_rev

    def on_redraw_timer(self,event ):
#         print "DataRead.PROC:", PROC
         if PROC == 1:
#          print "DataRead.PROC:", PROC
          self.draw_plot_rev()
          self.refreshCounter += 1
          #print "counter now is :", self.refreshCounter

    def create_status_bar(self):
        self.statusbar = self.CreateStatusBar()

    def draw_plot_rev(self):
#        print "Current PROC is: " ,DataRead.PROC
        #self.dataread.read()
        #Update the plot
        # import saved processed data MAY changed strob
        # want to plot them
        self.pros_sig_new = csv.reader(open("reconstruct.txt", "rU"),delimiter='\n')
        self.pros_sig_list_new = []
        self.pros_sig_list_new.extend(self.pros_sig_new)
        self.signal_rev_new = []
        for data in self.pros_sig_list_new:
         self.signal_rev_new.append(data[0])
          #print signal_rev_new

        def convertStr(s):
         try:
          ret = int(s)
         except ValueError:
          ret = float(s)
         return ret

        self.processed_sig_new = range(len(self.signal_rev_new))
        for i in range(len(self.signal_rev_new)):
         self.processed_sig_new[i] = 1.0*(convertStr(self.signal_rev_new[i]))
        
        #self.line.set_ydata(processed_sig_new)
        #print "read plot_new successfully"

        if self.strob == 9:# & control == 1:
         #self.line.set_ydata(processed_sig_new)
         self.fig_rev.clf()

         self.axes = self.fig_rev.add_subplot(111)
         self.axes.set_axis_bgcolor('black')
         self.axes.set_title('Processed ADC Values', size=12)
         self.axes.grid(True, color='gray')
#         self.axes.axis([0,buffer_for_DSP*x_length_factor,-1,3.6])   #Axes limits:([xmin,xmax,ymin,ymax])
         self.axes.axis([0, 1.0/230*buffer_for_DSP*x_length_factor,-0.5,1.2])   #Axes limits:([xmin,xmax,ymin,ymax])

         pylab.setp(self.axes.get_xticklabels(), fontsize=8)
         pylab.setp(self.axes.get_yticklabels(), fontsize=8)

#         self.axes.plot(range(buffer_for_DSP), self.processed_sig_new, label="PROCESSED SIG")
         x_unit = np.linspace(0,1.0/230*buffer_for_DSP,buffer_for_DSP)
         lineADD = self.axes.plot(x_unit, self.processed_sig_new, label="PROCESSED SIG")
         pylab.setp(lineADD, 'color', 'b', 'linewidth', 2.0)
         self.fig_rev.canvas.draw()
         global PROC
         PROC = 0
         self.strob = 0
         print "plot_new0 successfully"
        elif self.strob == 0:# & control == 1:
         #self.fig_rev.clf()
#         self.axes.plot(range(buffer_for_DSP,buffer_for_DSP*(x_length_factor-1)), self.processed_sig_new, label="PROCESSED SIG")
         x_unit_2 = np.linspace(1.0/230*buffer_for_DSP,1.0/230*buffer_for_DSP*2,buffer_for_DSP)
         lineADD = self.axes.plot(x_unit_2, self.processed_sig_new, label="PROCESSED SIG")
         pylab.setp(lineADD, 'color', 'b', 'linewidth', 2.0)
         self.fig_rev.canvas.draw()
         global PROC
         PROC = 0
         self.strob = 1
         print "plot_new1 successfully"
        elif self.strob == 1:# & control == 1:
         #self.fig_rev.clf()
#         self.axes.plot(range(buffer_for_DSP*(x_length_factor-1),buffer_for_DSP*x_length_factor), self.processed_sig_new, label="PROCESSED SIG")
         x_unit_3 = np.linspace(1.0/230*buffer_for_DSP*2,1.0/230*buffer_for_DSP*3,buffer_for_DSP)
         lineADD = self.axes.plot(x_unit_3, self.processed_sig_new, label="PROCESSED SIG")
         pylab.setp(lineADD, 'color', 'b', 'linewidth', 2.0)
         self.fig_rev.canvas.draw()
         global PROC
         PROC = 0
         self.strob = 2
         print "plot_new3 successfully"
        elif self.strob == 2:# & control == 1:
         #self.fig_rev.clf()
         x_unit_4 = np.linspace(1.0/230*buffer_for_DSP*3,1.0/230*buffer_for_DSP*4,buffer_for_DSP)
         lineADD = self.axes.plot(x_unit_4, self.processed_sig_new, label="PROCESSED SIG")
         pylab.setp(lineADD, 'color', 'b', 'linewidth', 2.0)
         self.fig_rev.canvas.draw()
         global PROC
         PROC = 0
         self.strob = 3
         print "plot_new4 successfully"
        elif self.strob == 3:# & control == 1:
         #self.fig_rev.clf()
         x_unit_5 = np.linspace(1.0/230*buffer_for_DSP*4,1.0/230*buffer_for_DSP*5,buffer_for_DSP)
         lineADD = self.axes.plot(x_unit_5, self.processed_sig_new, label="PROCESSED SIG")
         pylab.setp(lineADD, 'color', 'b', 'linewidth', 2.0)
         self.fig_rev.canvas.draw()
         global PROC
         PROC = 0
         self.strob = 4
         print "plot_new5 successfully"
        elif self.strob == 4:# & control == 1:
         #self.fig_rev.clf()
         x_unit_6 = np.linspace(1.0/230*buffer_for_DSP*5,1.0/230*buffer_for_DSP*6,buffer_for_DSP)
         lineADD = self.axes.plot(x_unit_6, self.processed_sig_new, label="PROCESSED SIG")
         pylab.setp(lineADD, 'color', 'b', 'linewidth', 2.0)
         self.fig_rev.canvas.draw()
         global PROC
         PROC = 0
         self.strob = 5
         print "plot_new6 successfully"
        elif self.strob == 5:# & control == 1:
         #self.fig_rev.clf()
         x_unit_7 = np.linspace(1.0/230*buffer_for_DSP*6,1.0/230*buffer_for_DSP*7,buffer_for_DSP)
         lineADD = self.axes.plot(x_unit_7, self.processed_sig_new, label="PROCESSED SIG")
         pylab.setp(lineADD, 'color', 'b', 'linewidth', 2.0)
         self.fig_rev.canvas.draw()
         global PROC
         PROC = 0
         self.strob = 6
         print "plot_new7 successfully"
        elif self.strob == 6:# & control == 1:
         #self.fig_rev.clf()
         x_unit_8 = np.linspace(1.0/230*buffer_for_DSP*7,1.0/230*buffer_for_DSP*8,buffer_for_DSP)
         lineADD = self.axes.plot(x_unit_8, self.processed_sig_new, label="PROCESSED SIG")
         pylab.setp(lineADD, 'color', 'b', 'linewidth', 2.0)
         self.fig_rev.canvas.draw()
         global PROC
         PROC = 0
         self.strob = 7
         print "plot_new8 successfully"
        elif self.strob == 7:# & control == 1:
         #self.fig_rev.clf()
         x_unit_9 = np.linspace(1.0/230*buffer_for_DSP*8,1.0/230*buffer_for_DSP*9,buffer_for_DSP)
         lineADD = self.axes.plot(x_unit_9, self.processed_sig_new, label="PROCESSED SIG")
         pylab.setp(lineADD, 'color', 'b', 'linewidth', 2.0)
         self.fig_rev.canvas.draw()
         global PROC
         PROC = 0
         self.strob = 8
         print "plot_new9 successfully"
        elif self.strob == 8:# & control == 1:
         #self.fig_rev.clf()
         x_unit_10 = np.linspace(1.0/230*buffer_for_DSP*9,1.0/230*buffer_for_DSP*10,buffer_for_DSP)
         lineADD = self.axes.plot(x_unit_10, self.processed_sig_new, label="PROCESSED SIG")
         pylab.setp(lineADD, 'color', 'b', 'linewidth', 2.0)
         self.fig_rev.canvas.draw()
         global PROC
         PROC = 0
         self.strob = 9
         print "plot_new10 successfully"
        else:
         #print "not this way!!!!!!"
         pass
##### need to use the completed data stream to enable refleshing data
        #self.line
#        if self.strob == 1:
#            self.canvas.draw()
#            self.strob = 0

    def create_sub_panel(self): 
        #Configure main window panel RECONSTRUCTED
        self.panel = wx.Panel(self)
        self.init_plot_rev()
        self.canvas = FigCanvas(self.panel, -1, self.fig_rev)

        self.save_button = wx.Button(self.panel, -1, "Save plot data")
        self.Bind(wx.EVT_BUTTON, self.on_save_button, self.save_button)
        #self.startADC_button= wx.Button(self.panel, -1, "    Initialize ADC    ")
        #self.Bind(wx.EVT_BUTTON, self.on_startADC, self.startADC_button)

        self.hbox1 = wx.BoxSizer(wx.HORIZONTAL)
        self.hbox1.Add(self.save_button, border=5, flag=wx.ALL | wx.ALIGN_CENTER_VERTICAL )
        #self.hbox1.Add(self.startADC_button, border=5, flag=wx.ALL | wx.ALIGN_CENTER_VERTICAL)

        self.vbox = wx.BoxSizer(wx.VERTICAL)
        self.vbox.Add(self.canvas, 1, flag=wx.LEFT | wx.TOP | wx.GROW)
        self.vbox.Add(self.hbox1, 0, flag=wx.ALIGN_RIGHT)

        self.panel.SetSizer(self.vbox)
        self.vbox.Fit(self)

    def on_save_button(self, event):
        print "save button was pressed"
        #Write the data to a file, launch a file dialog box
        file_choices = "CSV (*.csv)|*.csv"
        dlg = wx.FileDialog(
            self,
            message="Save plot data as...",
            defaultDir=os.getcwd(),
            defaultFile="Processed_ECG_data.csv",
            wildcard=file_choices,
            style=wx.SAVE | wx.OVERWRITE_PROMPT)

        if dlg.ShowModal() == wx.ID_OK: # it is passed requisit requirement to save the following format for col and rows.
            path = dlg.GetPath()
            self.p_data_array(path)

    def p_data_array(self, path):
	#----- want to get current plotted signal
        parsed_signal = self.processed_sig_new
        np.savetxt(path, parsed_signal)


    def init_plot_rev(self):
        self.dpi = 100
        self.fig_rev = Figure((5.0, 4.0), dpi=self.dpi)

        self.axes = self.fig_rev.add_subplot(111)
        self.axes.set_axis_bgcolor('black')
        self.axes.set_title('Processed ADC Values', size=12)
        self.axes.grid(True, color='gray')
#        self.axes.axis([0,buffer_for_DSP*x_length_factor,-1,3.6])   #Axes limits:([xmin,xmax,ymin,ymax])
        self.axes.axis([0, 1.0/230*buffer_for_DSP*x_length_factor,-0.5,1.2])   #Axes limits:([xmin,xmax,ymin,ymax])

        pylab.setp(self.axes.get_xticklabels(), fontsize=8)
        pylab.setp(self.axes.get_yticklabels(), fontsize=8)

        # plot the data as a line series, and save the reference
        # to the plotted line series

        # import saved processed data
        # want to plot them
        pros_sig = csv.reader(open("reconstruct.txt", "rU"),delimiter='\n')
        pros_sig_list = []
        pros_sig_list.extend(pros_sig)
        signal_rev = []
        for data in pros_sig_list:
         signal_rev.append(data[0])
          #print signal_rev

        def convertStr(s):
         try:
          ret = int(s)
         except ValueError:
          ret = float(s)
         return ret

        processed_sig = range(len(signal_rev))
        for i in range(len(signal_rev)):
         processed_sig[i] = convertStr(signal_rev[i])

        if (len(processed_sig) < buffer_for_DSP) == True: 
         for j in range(0,buffer_for_DSP-len(processed_sig)):
          processed_sig.append(0)


         pro_mod = processed_sig
        else:
         pro_mod = processed_sig[0:buffer_for_DSP]   # this is for the first time plot when buffer_for_DSP is changed



#        pylab.ion()

        # end loading the processed sig
        x_unit = np.linspace(0,1.0/230*buffer_for_DSP,buffer_for_DSP)
        self.line, = self.axes.plot(x_unit, pro_mod, label="PROCESSED SIG")
 #       for i in range(1,buffer_for_DSP):
 #           self.line.set_ydata(processed_sig[i])
 #           pylab.draw()                         # redraw the canvas
        self.axes.legend(loc='upper center',
            prop=matplotlib.font_manager.FontProperties(size='xx-small'),
            ncol=1)


if __name__ == '__main__':
    app = wx.PySimpleApp()
    app.frame = GraphFrame()
    app.frame.Show()
    app.frame_rev = GraphFrame_rev()
    app.frame_rev.Show()

    app.MainLoop()

