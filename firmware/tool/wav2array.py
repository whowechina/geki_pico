#  Copied from wav2array.py by Chris Veigl:
#  https://github.com/ChrisVeigl/ESP32Sound/blob/master/convertTool/wav2array.py
#  Copyright (C) 2019 Chris Veigl
#
#  convert .wav files to c-arrays (16KHz, mono, 8bit format)
#  usage: python wav2array.py file1.wav file2.wav ...
#  the converter will create the file sounds.h with respective c-array definitions.
#
#
#  thanks to:
#  https://stackoverflow.com/questions/30619740/python-downsampling-wav-audio-file
#


import sys
import os
import wave
import audioop

arguments=sys.argv
outFile ='sounds.h'
with open(outFile,'w') as out:
    out.write('#include <pgmspace.h>\n\n');
    for i in range(1, len(arguments)):
        fileName=arguments[i] 
        length = os.stat(fileName).st_size
        print ('Now processing file '+fileName+' with length '+str(length))
        try:
            s_read = wave.open(fileName, 'rb')
        except:
            print 'Failed to open file!'
            quit()

        n_frames = s_read.getnframes()
        data = s_read.readframes(n_frames)
        inchannels=s_read.getnchannels()
        bytes=s_read.getsampwidth()
        inrate=s_read.getframerate()
        print ('File has '+str(inchannels)+' channels,'+str(bytes)+' bytes per sample and rate '+str(inrate))

        try:
            print ('converting to 16KHz!')
            converted = audioop.ratecv(data, bytes, inchannels, inrate, 16000, None)
            if (inchannels == 2):
                print ('converting to mono!')
                converted = audioop.tomono(converted[0], bytes, 1, 0)
            if (bytes > 1):
                print ('converting to 8-bit representation!')
                converted = audioop.lin2lin(converted, bytes, 1)
                converted = audioop.bias(converted, 1, 128)
        except:
            print 'Failed to downsample wav'
            quit()

        try:
            s_read.close()
        except:
            print 'Failed to close wav file'
            quit()
        
        
        ascii = [ord(c) for c in converted]
        arrayName = fileName.partition(".")[0]
        cnt=0

        out.write('const uint8_t ')
        out.write(arrayName)
        out.write('[] PROGMEM={');

        length=len(converted)
        lowbyte= length & 0xff
        out.write(str(lowbyte)+' , ');
        length=length>>8
        lowbyte= length & 0xff
        out.write(str(lowbyte)+' , ');
        length=length>>8
        lowbyte= length & 0xff
        out.write(str(lowbyte)+' , ');
        length=length>>8
        lowbyte= length & 0xff
        out.write(str(lowbyte)+' , ');

        for row in ascii:
            out.write('{0},'.format(row))
            cnt=cnt+1
            if (cnt==50):
                out.write('\n')
                cnt=0
        out.write('};')
        out.write('\n\n')
    try:
        out.close()
    except:
        print 'Failed to close header file'