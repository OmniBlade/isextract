#include "isextract.h"

const uint32_t signature = 0x8C655D13;
const int32_t data_start = 255;

InstallShield::InstallShield(std::string& filename):
m_filename(filename)
{
    uint32_t sig;
    int32_t toc_address;
    uint16_t dir_count;
    
    m_fh.open(filename, std::ios::binary|std::ios::in);
    m_fh.read(reinterpret_cast<char*>(&sig), sizeof(uint32_t));
    m_fh.seekg(37, std::ios_base::cur);
    m_fh.read(reinterpret_cast<char*>(&toc_address), sizeof(int32_t));
    m_fh.seekg(4, std::ios_base::cur);
    m_fh.read(reinterpret_cast<char*>(&dir_count), sizeof(uint16_t));
    
    if(sig != signature)
        throw "Not an Installshield package";

    reader.ReadBytes(8);
    /*var FileCount = */reader.ReadUInt16();
    reader.ReadBytes(4);
    /*var ArchiveSize = */reader.ReadUInt32();
    reader.ReadBytes(19);
    var TOCAddress = reader.ReadInt32();
    reader.ReadBytes(4);
    var DirCount = reader.ReadUInt16();

    // Parse the directory list
    s.Seek(TOCAddress, SeekOrigin.Begin);
    var TOCreader = new BinaryReader(s);

    var fileCountInDirs = new List<uint>();
    // Parse directories
    for (var i = 0; i < DirCount; i++)
            fileCountInDirs.Add(ParseDirectory(TOCreader));

    // Parse files
    foreach (var fileCount in fileCountInDirs)
            for (var i = 0; i < fileCount; i++)
                    ParseFile(reader);
}


		static uint ParseDirectory(BinaryReader reader)
		{
			// Parse directory header
			var FileCount = reader.ReadUInt16();
			var ChunkSize = reader.ReadUInt16();
			var NameLength = reader.ReadUInt16();
			reader.ReadChars(NameLength); //var DirName = new String(reader.ReadChars(NameLength));

			// Skip to the end of the chunk
			reader.ReadBytes(ChunkSize - NameLength - 6);
			return FileCount;
		}

		uint AccumulatedData = 0;
		void ParseFile(BinaryReader reader)
		{
			reader.ReadBytes(7);
			var CompressedSize = reader.ReadUInt32();
			reader.ReadBytes(12);
			var ChunkSize = reader.ReadUInt16();
			reader.ReadBytes(4);
			var NameLength = reader.ReadByte();
			var FileName = new String(reader.ReadChars(NameLength));

			var hash = PackageEntry.HashFilename(FileName, PackageHashType.Classic);
			if (!index.ContainsKey(hash))
				index.Add(hash, new PackageEntry(hash,AccumulatedData, CompressedSize));
			filenames.Add(FileName);
			AccumulatedData += CompressedSize;

			// Skip to the end of the chunk
			reader.ReadBytes(ChunkSize - NameLength - 30);
		}

		public Stream GetContent(uint hash)
		{
			PackageEntry e;
			if (!index.TryGetValue(hash, out e))
				return null;

			s.Seek(dataStart + e.Offset, SeekOrigin.Begin);
			var data = s.ReadBytes((int)e.Length);

			return new MemoryStream(Blast.Decompress(data));
		}

		public Stream GetContent(string filename)
		{
			return GetContent(PackageEntry.HashFilename(filename, PackageHashType.Classic));
		}

		public IEnumerable<uint> ClassicHashes()
		{
			return index.Keys;
		}

		public IEnumerable<uint> CrcHashes()
		{
			yield break;
		}

		public IEnumerable<string> AllFileNames()
		{
			return filenames;
		}

		public bool Exists(string filename)
		{
			return index.ContainsKey(PackageEntry.HashFilename(filename, PackageHashType.Classic));
		}

		public int Priority { get { return 2000 + priority; }}
		public string Name { get { return filename; } }

		public void Write(Dictionary<string, byte[]> contents)
		{
			throw new NotImplementedException("Cannot save InstallShieldPackages.");
		}
        }


