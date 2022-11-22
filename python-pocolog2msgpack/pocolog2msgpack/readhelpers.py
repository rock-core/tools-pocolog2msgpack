import msgpack
from .BufferedFileDecompressor import BufferedFileDecompressor
import numpy as np

def get_fields(filename, decompress="auto"):    
    
    if decompress=="zlib" or (decompress=="auto" and filename.endswith(".zz")):
        f = BufferedFileDecompressor(filename)
        f.open()
    else:
        f = open(filename, 'rb')
    
    paths = []
        
    try:
        unpacker = msgpack.Unpacker(f)
        
        key_count = unpacker.read_map_header()
        for i in range(key_count):
            paths += [unpacker.unpack()]
            unpacker.skip()
        
        del unpacker
    finally:
        f.close()
        
    del f
    
    return paths
    
def read_field(filename, srcKey, dataToNumpyArray=True, decompress="auto"):
    return read_fields( filename, srcKeys=[srcKey], dataToNumpyArray=dataToNumpyArray )[srcKey]

def read_fields(filename, srcKeys=None, renameMap=None, dataToNumpyArray=True, allowDuplicates=False, decompress="auto"):
    """
    Read the list of fields in srcKeys from compress msgpack file determined by filename.
        
    :param filename: the path of the input file (e.g. a *.msg.zz file)
    :param srcKeys: the list of fields (to level keys) to read or None to read all or can be the same as renameMap
    :param renameMap: a dict listing how the srcKeys should be called in the output
    :param dataToNumpyArray: if true, each field is converted to numpy array
    :param allowDuplicates: if true, values of output dict become lists
    :returns: a dict with the field names or the values of renameMap as keys, and the data as values
    :raises IndexError: raises an exception a key cannot be found or is not unique
    """
    
    if decompress=="zlib" or (decompress=="auto" and filename.endswith(".zz")):
        f = BufferedFileDecompressor(filename)
        f.open()
    else:
        f = open(filename, 'rb')
    
    wanted_fields = None
    
    if isinstance(srcKeys, list):
        wanted_fields = srcKeys.copy()
    elif isinstance(srcKeys, dict):
        wanted_fields = list(srcKeys.keys())
    elif srcKeys!=None:
        wanted_fields += [srcKeys]
        
    field_data = {}
    
    try:
        unpacker = msgpack.Unpacker(f)
        key_count = unpacker.read_map_header()        
        
        while key_count > 0:
            field_name = unpacker.unpack()
            print("Next key is ", field_name)
            
            if wanted_fields==None or wanted_fields.count(field_name)>0:
                if dataToNumpyArray:
                    cur_data = np.array(unpacker.unpack())
                else:
                    cur_data = unpacker.unpack()
                    
                target_name = field_name
                if renameMap is not None and field_name in renameMap:
                    target_name = renameMap[field_name]
                    
                if target_name in field_data:
                    if allowDuplicates:
                        field_data[target_name].append( cur_data )                            
                    else:
                        raise RuntimeError("Key {} not unique and allowDuplicates is set to False.".format(f))
                else:
                    if allowDuplicates:
                        field_data[target_name] = [ cur_data ]
                    else:
                        field_data[target_name] = cur_data
                
            else:
                print("....skipped")
                unpacker.skip()
            
            key_count = key_count - 1
            
        if wanted_fields != None:
            for fld in wanted_fields:
                target_name = fld
                if renameMap is not None and fld in renameMap:
                    target_name = renameMap[fld]
                    
                if target_name not in field_data:
                    print("Availabe keys: ", field_data.keys())
                    raise IndexError("Did not find requested field \"{}\"->\"{}\".".format(fld, target_name))
            
    finally:
        f.close()
        
    del unpacker
    del f
    
    return field_data
 
