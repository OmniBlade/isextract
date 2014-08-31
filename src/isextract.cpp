#include "isextract.h"

#include <iostream>

const uint32_t signature = 0x8C655D13;
const int32_t data_start = 255;
const uint32_t CHUNK = 16384;

unsigned inf(void *how, unsigned char **buf)
{
    static unsigned char hold[CHUNK];

    *buf = hold;
    return fread(hold, 1, CHUNK, (FILE *)how);
}

int outf(void *how, unsigned char *buf, unsigned len)
{
    return fwrite(buf, 1, len, (FILE *)how) != len;
}

InstallShield::~InstallShield()
{
    
}

InstallShield::InstallShield():
m_dataoffset(data_start),
m_datasize(0)
{
    
}

void InstallShield::open(std::string& filename)
{
    uint32_t sig;
    int32_t toc_address;
    uint16_t dir_count;
    std::streampos pretoc;
    m_filename = std::string(filename);
    
    m_fh.open(filename.c_str(), std::ios::binary|std::ios::in);
    
    if(!m_fh.is_open())
        throw "Could not open file.";
    
    m_fh.read(reinterpret_cast<char*>(&sig), sizeof(uint32_t));
    
    //test if we have what we think we have
    if(sig != signature)
        throw "Not a valid InstallShield 3 archive.";
    
    //get some basic info on where stuff is in file
    m_fh.seekg(37, std::ios_base::cur);
    m_fh.read(reinterpret_cast<char*>(&toc_address), sizeof(int32_t));
    m_fh.seekg(4, std::ios_base::cur);
    m_fh.read(reinterpret_cast<char*>(&dir_count), sizeof(uint16_t));
    
    //find the toc and work out how many files we have in the archive
    m_fh.seekg(toc_address, std::ios_base::beg);
    
    std::vector<uint32_t> dir_files;
    
    for(uint32_t i = 0; i < dir_count; i++) {
        dir_files.push_back(parseDirs());
    }

    //parse the file entries in the toc to get filenames, size and location
    for(uint32_t i = 0; i < dir_files.size(); i++){
        for(uint32_t j = 0; j < dir_files[i]; j++) {
            parseFiles();
        }
    }
    
    m_fh.close();
}

void InstallShield::close()
{
    m_filename = "";
}

uint32_t InstallShield::parseDirs()
{
    uint16_t fcount;
    uint16_t chksize;
    uint16_t nlen;
    
    m_fh.read(reinterpret_cast<char*>(&fcount), sizeof(uint16_t));
    m_fh.read(reinterpret_cast<char*>(&chksize), sizeof(uint16_t));
    m_fh.read(reinterpret_cast<char*>(&nlen), sizeof(uint16_t));
    
    std::cout << "We have " << fcount << " files\n";
    
    //skip the name of the dir, we just want the files
    m_fh.seekg(nlen, std::ios_base::cur);
    
    //skip to end of chunk
    m_fh.seekg(chksize - nlen - 6, std::ios_base::cur);

    return fcount;
}

//uint AccumulatedData = 0;
void InstallShield::parseFiles()
{
    t_file_entry file;
    uint16_t chksize;
    uint8_t namelen;
    
    m_fh.seekg(7, std::ios_base::cur);
    m_fh.read(reinterpret_cast<char*>(&file.second.compressed_size), sizeof(uint32_t));
    m_fh.seekg(12, std::ios_base::cur);
    m_fh.read(reinterpret_cast<char*>(&chksize), sizeof(uint16_t));
    m_fh.seekg(4, std::ios_base::cur);
    m_fh.read(reinterpret_cast<char*>(&namelen), sizeof(uint8_t));
    
    //read in file name, ensure null termination;
    uint8_t buffer[namelen + 1];
    m_fh.read(reinterpret_cast<char*>(buffer), namelen);
    buffer[namelen] = '\0';
    file.first = reinterpret_cast<char*>(buffer);
    
    //complete out file entry with the offset within the body.
    file.second.offset = m_datasize;
    
    m_files.insert(file);
    
    //increase body size to next offset for next file
    m_datasize += file.second.compressed_size;
    
    //skip to end of chunk
    m_fh.seekg(chksize - namelen - 30, std::ios_base::cur);
}

bool InstallShield::extractFile(const std::string& filename, const std::string& dir)
{
    t_file_entry file;
    FILE* ifh;
    FILE* ofh;
    
    m_current_file = m_files.find(filename);
    
    if(m_current_file != m_files.end()) {
        file = *m_current_file;
    } else {
        return false;
    }
    
    ifh = fopen(m_filename.c_str(), "rb");
    ofh = fopen((dir + DIR_SEPARATOR + filename).c_str(), "wb");
    
    if(!ifh || !ofh) return false;
    
    //seek to the file position we want to extract
    //m_fh.seekg(m_current_file->second.offset + m_dataoffset, std::ios_base::beg);
    
    fseek(ifh, m_current_file->second.offset + m_dataoffset, SEEK_SET);
    
    //open an output file stream
    //ofh.open((dir + DIR_SEPARATOR + filename).c_str(), std::fstream::out|std::fstream::binary);
    
    //todo Blast decompress
    
    blast(inf, ifh, outf, ofh);
    
    fclose(ifh);
    fclose(ofh);
    
    return true;
}

bool InstallShield::extractAll(const std::string& dir)
{
    t_file_iter it = m_files.begin();
    bool rv = true;
    
    while(it != m_files.end()) {
        if(!extractFile(it->first, dir)) rv = false;
        
        it++;
    }
    
    return rv;
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
