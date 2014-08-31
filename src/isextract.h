/* 
 * File:   isextract.h
 * Author: aidan
 *
 * Created on 28 August 2014, 23:29
 */

#ifndef ISEXTRACT_H
#define	ISEXTRACT_H

#include "blast.h"
#include <stdint.h>
#include <string>
#include <vector>
#include <fstream>
#include <map>

class InstallShield
{
public:
    InstallShield();
    ~InstallShield();
    void open(std::string& filename);
    void close();
    void listFiles();
    bool extractFile(const std::string& filename, const std::string& dir);
    bool extractAll(const std::string& dir);
private:
    struct t_entry {
        uint32_t compressed_size;
        uint32_t offset;
    };
    typedef std::map<std::string, t_entry> t_file_map;
    typedef std::pair<std::string, t_entry> t_file_entry;
    typedef std::map<std::string, t_entry>::const_iterator t_file_iter;
    
    uint32_t parseDirs();
    void parseFiles();
    Blast m_decomp;
    t_file_map m_files;
    std::vector<std::string> m_filenames;
    std::string m_filename;
    std::fstream m_fh;
    uint32_t m_dataoffset;
    uint32_t m_datasize;
    t_file_iter m_current_file;
    int32_t m_file_remaining;
};

#endif	/* ISEXTRACT_H */

