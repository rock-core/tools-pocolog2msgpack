import msgpack
import zlib
from .FifoFileBuffer import FifoFileBuffer

class BufferedFileDecompressor:

    CHUNKSIZE = 32*1024
    
    def __init__(self, path):
        #print("BufferedFileDecompressor.__init__({})".format(path))
        self.fp = None
        self.path = ""
        self.dec = None
        self.dat = FifoFileBuffer()
        self.done = False
        self.seek_pos = 0
        
        self.path = path
        self.dec = zlib.decompressobj()
        self.seek_pos = 0
        
    def open(self):
        if not self.fp:
            self.fp = open(self.path, "rb")
            self.fp.seek(0)
    
    def close(self):
        if self.fp:
            self.fp.close()
            self.fp = None
            
    def read(self, n):
        while (self.dat.available < n and not self.done):
            buf = self.fp.read(self.CHUNKSIZE)
        
            if buf:
                decompressed_data = self.dec.decompress(buf)            
                self.dat.write(decompressed_data)
            elif not self.done:
                self.done = True
                decompressed_data = self.dec.flush()
                self.dat.write(decompressed_data)
            
            
        self.seek_pos += min(self.dat.available, n)
        return self.dat.read(n)
        
    def seek(self, n):
        if n < self.seek_pos:
            self.reset()
        
        while self.seek_pos < n:
            
            if self.done:
                raise IndexError("Target seek position beyond stream end.")
                
            self.read(min(n-self.seek_pos, self.CHUNKSIZE))
            
                
    def reset(self):
        
        del self.dec
        self.dec = zlib.decompressobj()
        del self.dat
        self.dat = FifoFileBuffer()
        self.seek_pos = 0
        self.done = False
            
