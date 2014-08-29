#include "isextract.h"
#include "blast.h"

#include <iostream>

const uint32_t signature = 0x8C655D13;
const int32_t data_start = 255;

InstallShield::InstallShield(std::string& filename):
m_filename(filename),
m_dataoffset(255),
m_datasize(0)
{
    uint32_t sig;
    int32_t toc_address;
    uint16_t dir_count;
    
    /*reader.ReadBytes(8);
    var FileCount = reader.ReadUInt16();
    reader.ReadBytes(4);
    var ArchiveSize = reader.ReadUInt32();
    reader.ReadBytes(19);
    var TOCAddress = reader.ReadInt32();
    reader.ReadBytes(4);
    var DirCount = reader.ReadUInt16();*/
    
    m_fh.open(filename.c_str(), std::ios::binary|std::ios::in);
    m_fh.read(reinterpret_cast<char*>(&sig), sizeof(uint32_t));
    
    //test if we have what we think we have
    if(sig != signature)
        throw "Not an Installshield package";
    
    m_fh.seekg(37, std::ios_base::cur);
    m_fh.read(reinterpret_cast<char*>(&toc_address), sizeof(int32_t));
    m_fh.seekg(4, std::ios_base::cur);
    m_fh.read(reinterpret_cast<char*>(&dir_count), sizeof(uint16_t));
    
    /* Parse the directory list
    s.Seek(TOCAddress, SeekOrigin.Begin);
    var TOCreader = new BinaryReader(s);

    var fileCountInDirs = new List<uint>();
    // Parse directories
    for (var i = 0; i < DirCount; i++)
            fileCountInDirs.Add(ParseDirectory(TOCreader)); */
    
    m_fh.seekg(toc_address, std::ios_base::beg);
    
    std::vector<uint32_t> dir_files;
    
    for(uint32_t i = 0; i < dir_count; i++) {
        dir_files.push_back(parseDirs());
    }

    /* Parse files
    foreach (var fileCount in fileCountInDirs)
            for (var i = 0; i < fileCount; i++)
                    ParseFile(reader);*/
    for(uint32_t i = 0; i < dir_files.size(); i++){
        for(uint32_t j = 0; j < i; j++) {
            parseFiles();
        }
    }
}

uint32_t InstallShield::parseDirs()
{
    uint16_t fcount;
    uint16_t chksize;
    uint16_t nlen;
    
    /* Parse directory header
        var FileCount = reader.ReadUInt16();
        var ChunkSize = reader.ReadUInt16();
        var NameLength = reader.ReadUInt16();
        reader.ReadChars(NameLength); //var DirName = new String(reader.ReadChars(NameLength));*/
    
    m_fh.read(reinterpret_cast<char*>(&fcount), sizeof(uint16_t));
    m_fh.read(reinterpret_cast<char*>(&chksize), sizeof(uint16_t));
    m_fh.read(reinterpret_cast<char*>(&nlen), sizeof(uint16_t));
    
    //skip the name of the dir, we just want the files
    m_fh.seekg(nlen, std::ios_base::cur);
    
    //skip to end of chunk
    m_fh.seekg(chksize - nlen - 6, std::ios_base::cur);

    return fcount;
}

//uint AccumulatedData = 0;
void InstallShield::parseFiles()
{
    
    /*reader.ReadBytes(7);
    var CompressedSize = reader.ReadUInt32();
    reader.ReadBytes(12);
    var ChunkSize = reader.ReadUInt16();
    reader.ReadBytes(4);
    var NameLength = reader.ReadByte();
    var FileName = new String(reader.ReadChars(NameLength));
    filenames.Add(FileName);
    AccumulatedData += CompressedSize;

    // Skip to the end of the chunk
    reader.ReadBytes(ChunkSize - NameLength - 30); */
    
    t_file_entry file;
    uint16_t chksize;
    uint8_t namelen;
    
    m_fh.seekg(7, std::ios_base::cur);
    m_fh.read(reinterpret_cast<char*>(&file.second.compressed_size), sizeof(uint32_t));
    m_fh.seekg(12, std::ios_base::cur);
    m_fh.read(reinterpret_cast<char*>(&chksize), sizeof(uint16_t));
    m_fh.seekg(4, std::ios_base::cur);
    m_fh.read(reinterpret_cast<char*>(&namelen), sizeof(uint8_t));
    
    uint8_t buffer[namelen];
    m_fh.read(reinterpret_cast<char*>(buffer), namelen);
    file.first = reinterpret_cast<char*>(buffer);
    //m_filenames.push_back(file.first);
    file.second.offset = m_datasize;
    
    m_files.insert(file);
    
    m_datasize += file.second.compressed_size;
    
    //skip to end of chunk
    m_fh.seekg(chksize - namelen - 30, std::ios_base::cur);
}

bool InstallShield::extractFile(const std::string& filename)
{
    t_file_entry file;
    std::fstream ofh;
    
    t_file_iter info = m_files.find(filename);
    
    if(info != m_files.end()) {
        file = *info;
    } else {
        return false;
    }

    ofh.open(filename.c_str(), std::fstream::out|std::fstream::binary);
    
    //todo Blast decompress
    
    return true;
}

bool InstallShield::extractAll(const std::string& dir)
{
    std::fstream ofh;
    t_file_iter info;
    
    return true;
}

void InstallShield::listFiles()
{
    t_file_iter it = m_files.begin();
    std::string fname;
    
    std::cout << "Archive contains the following files: \n";
    
    while(it != m_files.end()) {
        //try to get a filename, if lmd doesn't have it try gmd.
        fname = it->first;
        
        std::cout << fname << "\n";
        
        it++;
    }
}
