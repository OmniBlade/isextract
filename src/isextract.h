/* 
 * File:   isextract.h
 * Author: aidan
 *
 * Created on 28 August 2014, 23:29
 */

#ifndef ISEXTRACT_H
#define	ISEXTRACT_H

#include <stdint.h>
#include <string>
#include <vector>
#include <fstream>

class InstallShield
{
public:
    InstallShield(std::string& filename);
    ~InstallShield();
private:
    uint32_t parseDirs();
    uint32_t parseFiles();
    std::vector<std::string> m_filenames;
    const std::string m_filename;
    std::fstream m_fh;
    
};

#endif	/* ISEXTRACT_H */

