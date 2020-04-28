import struct
import sys
import numpy as np
from enum import IntEnum

class RoaConsts(IntEnum):
	MAX_SAMPLES             = 1024
	DATA_FILE_VERSION       = 1
	SECTION_SYNC_BYTE       = 0x40
	LOCAL_DATA_SECTION_ID   = 0x1111
	REMOTE_DATA_SECTION_ID  = 0x2222
	STATS_SECTION_ID        = 0x3333
	DESCR_SECTION_ID        = 0x4444
	FILE_SYNC_SEQ           = 0x414B53

class RoaFileHeader:
    def fromfile(self, fd):
      buffer = fd.read(struct.calcsize('<b3x'))
      self.VersionNum, = struct.unpack('<b3x', buffer)
      return self

class RoaHeader:
    def fromfile(self, fd):
      buffer = fd.read(struct.calcsize('<hbbi'))
      (self.SectionID,
        self.VersionNum,
        self.SyncByte,
        self.SectionLen) = struct.unpack('<hbbi', buffer)
      return self

#   SNAPSHOT_FILE_HEADER snapshotFileHead;
#   SNAPSHOT_HEADER snapshotDataHead;
#   float ROAspecta[MAX_SAMPLES];
#   float RAMANspecta[MAX_SAMPLES];
#   double LeftHand[2][2][2][MAX_SAMPLES];
#   double RightHand[2][2][2][MAX_SAMPLES];
#   float ROA_CC[MAX_SAMPLES];
#   float ROAnoiseFloor[MAX_SAMPLES];
#   SNAPSHOT_HEADER snapshotStatsHead;
#   int dataFileVersion;
#   int numberOfScans; 		// au depart long
#   double elapsedTime;		// au depart float
#   double TotalExposureTime;

def c2py_string(cstring):
  "Convertit une suite d'octets provenaire d'une chaine C en string python"
  s = ""
  for i in range(len(cstring)):
    if cstring[i] == 0:
      break
    s += chr(cstring[i])
  return s


class RoaData:
    def __init__(self):
      self.file_header = RoaFileHeader()
      self.file_header.VersionNum = 0
      self.RoaSpecta = ()
      self.RamanSpecta = ()
      self.LeftHand = ()
      self.RightHand = ()
      self.roa_cc = ()
      self.roaNoise = ()
      self.dataFileVersion = 0
      self.numberOfScans = 0
      self.elapsedTime = 0.0
      self.TotalExposureTime = 0.0
      self.name = ""
      self.description = ""

      self.fname = ""
      
    
    def open(self, fname):
      self.file_header.VersionNum = 0
      try:
        with open(fname, 'rb') as fd:
          self.fname = fname
          self.file_header = self.file_header.fromfile(fd)
  
          h = RoaHeader()
          h = h.fromfile(fd)
          if h.SectionID == int(RoaConsts.LOCAL_DATA_SECTION_ID):
            self.data_header = h
            # "Section de données, la longueur correcte de la section est dans h.SectionLen"
            buffer = fd.read(h.SectionLen)
            pos = 0
            self.RoaSpecta = struct.unpack_from('<' + str(int(RoaConsts.MAX_SAMPLES)) +'f', buffer, offset = pos)
            l = struct.calcsize('<' + str(int(RoaConsts.MAX_SAMPLES)) +'f')
            pos += l
            self.RamanSpecta = struct.unpack_from('<' + str(int(RoaConsts.MAX_SAMPLES)) +'f', buffer, offset = pos)
            l = struct.calcsize('<' + str(int(RoaConsts.MAX_SAMPLES)) +'f') 
            pos += l
            self.LeftHand = struct.unpack_from('<' +  str(2*2*2*int(RoaConsts.MAX_SAMPLES)) +'d', buffer, offset = pos)
            l = struct.calcsize('<' + str(2*2*2*int(RoaConsts.MAX_SAMPLES)) +'d') 
            pos += l
            self.RightHand = struct.unpack_from('<' + str(2*2*2*int(RoaConsts.MAX_SAMPLES)) +'d', buffer, offset = pos)
            l = struct.calcsize('<' + str(2*2*2*int(RoaConsts.MAX_SAMPLES)) +'d') 
            pos += l
            self.roa_cc = struct.unpack_from('<' + str(int(RoaConsts.MAX_SAMPLES)) +'f', buffer, offset = pos)
            l = struct.calcsize('<' + str(int(RoaConsts.MAX_SAMPLES)) +'f') 
            pos += l
            self.roaNoise = struct.unpack_from('<' + str(int(RoaConsts.MAX_SAMPLES)) +'f', buffer, offset = pos)
            l = struct.calcsize('<' + str(int(RoaConsts.MAX_SAMPLES)) +'f') 
            pos += l
  
          h = h.fromfile(fd)
          if h.SectionID == int(RoaConsts.STATS_SECTION_ID):
            self.stats_header = h
            "Section des statistiques, la longueur correcte de la section n'est pas dans h.SectionLen"
            "la section est plus grande que SectionLen"
            buffer = fd.read(struct.calcsize('<iidd'))
             
            (
              self.dataFileVersion,
              self.numberOfScans,
              self.elapsedTime,
              self.TotalExposureTime
            ) = struct.unpack('<iidd', buffer)
            
          h = h.fromfile(fd)
          if h.SectionID == int(RoaConsts.DESCR_SECTION_ID):
            self.descr_header = h
            buffer = fd.read(100)
            self.name = c2py_string(buffer)
            buffer = fd.read(2000)
            self.description = c2py_string(buffer)
  
        fd.close()
      except OSError as err:
        print("Error: {} loading file : {}\n".format(err, fname))
      
      except :
        print("Error : bad format in  file {}".format(fname))

      return self

    def getLeftHand(self,a, b, c, d):
      pos = ((a*2+b)*2+c) * int(RoaConsts.MAX_SAMPLES) + d
      if pos < len(self.LeftHand):
        return self.LeftHand[ pos]
      return -1.0  

    def getRightHand(self,a, b, c, d):
      pos = ((a*2+b)*2+c) * int(RoaConsts.MAX_SAMPLES) + d
      if pos < len(self.RightHand):
        return self.RightHand[ pos]
      return -1.0  
    
    def getRoaSpecta(self, pos):
      if pos < len(self.RoaSpecta):
        return self.RoaSpecta[pos]
      return -1.0

    def getRamanSpecta(self, pos):
      if pos < len(self.RamanSpecta):
        return int(self.RamanSpecta[pos])
      else:
        return -1
        
    def getRoaCC(self, pos):
      if pos < len(self.roa_cc):
        return self.roa_cc(pos)
      return -1.0

    def getRoaNoise(self, pos):
      if pos < len(self.roaNoise):
        return int(self.roaNoise)
      else:
        return -1
    
    def getNumberOfScans(self):
      return self.numberOfScans
    
    def getElapsedTime(self):
      return self.elapsedTime
      
    def getTotalExposureTime(self):
      return self.TotalExposureTime
    
    def getName(self):
      return self.name
    
    def getDescription(self):
      return self.description

    def getFileName(self):
      return self.fname
    
    def affiche(self, maximum = 0):
      if maximum > 0 :
            print('{:^4s};{:^15s};{:^15s}'.format('pos','raman', 'roa'))
      for i in range(min(maximum, int(RoaConsts.MAX_SAMPLES))):
        print('{:4d};{:15d};{:^11.4f}'.format(i,self.getRamanSpecta(i), self.getRoaSpecta(i)))

    def affiche_stats(self):
        print("{:^20s};{:6d};{:^8.4f};{:^8.4f}".format( self.name, self.numberOfScans, self.elapsedTime, self.TotalExposureTime))

import argparse
import os

def main(argv=sys.argv[1:]):
  parser = argparse.ArgumentParser(description='Process some ROA .dat files.')
  parser.add_argument('--showdata', default=False, action='store_true', help='Shown Content data')
  parser.add_argument('--showdesc', default=False, action='store_true', help='Shown Description')
  parser.add_argument('--showstats', default=False, action='store_true', help='Shown Statistics')
  parser.add_argument('-n', default=0, type=int, action='store', help='# elements to print')
  parser.add_argument('files', metavar='file', type=str, nargs='*',
                    help='show ROA content file')

  args = parser.parse_args()


  roa = RoaData()

  print(args)

  for fname in args.files:
    if not os.path.exists(fname):
        print("error: {} doesn't exist\n".format(fname))
    roa = roa.open(fname)
    # print(roa.getLeftHand(1,0,1,512))
    # print(roa.getName())
    if args.showdesc:
        print(roa.getDescription())
    if args.showstats:
        roa.affiche_stats()
    if args.showdata:
        roa.affiche(args.n)

if __name__ == '__main__':
    main()


