#include "CdrProcessor.h"

namespace experis
{

CdrProcessor_mt::CdrProcessor_mt(const FolderPath& a_folderInputPath, const FolderPath& a_afterProcessFolderPath, DatabaseOperations_mt& a_db)
: m_filesProcessDirPath{a_folderInputPath}
, m_doneProcessFolderPath{a_afterProcessFolderPath}
, m_cdrsFilesPath{}
, m_cdrsToProcess{}
, m_db{a_db}
{
}

void CdrProcessor_mt::GetDirFilesToProcess()
{
    namespace fs = boost::filesystem; 

    // assuming only one input Dir
    fs::path targetDir(m_filesProcessDirPath);
    fs::directory_iterator it(targetDir), eod;

    while (true) //Todo windows notify file added to file to prevent busy waiting
    {
        {  
            std::unique_lock<std::mutex> insertFileslocker (m_queueFilesPath);

            while(m_cdrsFilesPath.IsFull_not_mt())
            {
                m_waitForFileTransfer.wait(insertFileslocker);
            }

            BOOST_FOREACH(fs::path const &p, std::make_pair(it, eod))   
            { 
                if (fs::is_regular_file(p))
                {
                    m_cdrsFilesPath.Enqueue(p.filename().string());
                    m_waitForFileTransfer.notify_all();
                } 
            }
        }
    }
}

void CdrProcessor_mt::EnqueueEachFileToProcess()
{
    FilePath fileName{};
	
    while(true)
    {
        std::unique_lock<std::mutex> getFileslocker (m_queueFilesPath);
        {
            while (m_cdrsFilesPath.IsVacant_not_mt())
            {
                m_waitForFileTransfer.wait(getFileslocker);
            }
            m_cdrsFilesPath.Dequeue(fileName);
        }   
        m_waitForFileTransfer.notify_all();
        std::string filePath = m_filesProcessDirPath + "\\" + fileName;
        std::ifstream file(filePath);
        std::string cdr{};
    
        assert (filePath != ""); //Todo add check if file exist with that path!!! else throw exeption etc

        while (std::getline(file, cdr)) // check if file exist, than check if cdr exist...
        {
            std::unique_lock<std::mutex> insertCdrslocker (m_queueCdrsToProcess);
            {
                while (m_cdrsToProcess.IsFull_not_mt())
                {
                    m_waitForCdrTransfer.wait(insertCdrslocker);
                }
                m_cdrsToProcess.Enqueue(cdr); // It would wait for Dequeue inside queue, no need to add wait here
            }
            m_waitForCdrTransfer.notify_all(); //To make sure that even if queue is full potentially, it would notify and won't stuck inside...
            //std::getline(file, cdr);
        }
        std::error_code err;
        std::string destinationPath = m_doneProcessFolderPath.erase(m_doneProcessFolderPath.size() - 1) + "\\" + fileName;
        file.close();
        std::filesystem::rename(filePath, destinationPath, err);
        if (err)
        {
            std::cerr << "Error moving file: " << err.message() << "\n";
        }
        else
        {
            // Todo logger?
            //std::cout << "File moved successfully" << std::endl;
        }
    }
}

/// <summary>
/// get cdr line by line as string, part it to relevant data, process it's third option (explained below), than insert to Database:
/// 
/// Meaning: id |Sequence IMSI  |          IMEI    |option|    MSISDN   | date     | time   |duration[s]|bytes received |bytes transmitted| Second Party MSISDN
/// Cdr line: 1 |425020528409018|35-209900-176148-4| MTC  |9720528409043|28/05/2023|03:45:12|240        |0              |0                | 9720528409042
/// I get:      |425020524050930|                  |option|972524050930 |          |        |120        |0              |0                | 972527214488
/// 
/// option are one of:
/// MOC    outgoing voice call
/// MTC    incoming voice call
/// SMS-MO outgoing message
/// SMS-MT incoming message
/// D      Data
/// U      call not answered
/// B      busy call
/// X      failed call
/// 
/// </summary>
void CdrProcessor_mt::ProcessCdrsInsideFile()
{
    while(true)
    {
        CdrLine cdr;
        {
            std::unique_lock<std::mutex> insertCdrslocker (m_queueCdrsToProcess);
            while(m_cdrsToProcess.IsVacant_not_mt())
            {
                m_waitForCdrTransfer.wait(insertCdrslocker);
            }
            m_cdrsToProcess.Dequeue(cdr);
        }
        std::vector<unsigned short> delimiterIdx{};
        GetDelimiterIdx(delimiterIdx, '|', cdr);
        if (delimiterIdx.size() != 10)
        {
            assert(!"wrong size of Cdr inside ProcessFilesInsideFolder function");
        }
        std::vector<std::string> cdrProcessedData{};
        
        // V0 IMSI,v1 option, v2 MSISDN, v3 duration, v4 bytes recieved , v5 bytes transmited, v6 Second Party MSISDN
        //Todo rethink if duration actually needed, as volume could be understand via sms and calls as well, it depends, currently, would go without duration.
        // + (duration for sms doesn't make sense, hence I might be correct).
        cdrProcessedData.push_back(cdr.substr(size_t(delimiterIdx[0]) + PLUS_ONE_AFTER_DELIMITER, size_t(delimiterIdx[1]) - delimiterIdx[0] - MINUS_ONE_BEFORE_DELIMITER));
        cdrProcessedData.push_back(cdr.substr(size_t(delimiterIdx[2]) + PLUS_ONE_AFTER_DELIMITER, size_t(delimiterIdx[3]) - delimiterIdx[2] - MINUS_ONE_BEFORE_DELIMITER));
        cdrProcessedData.push_back(cdr.substr(size_t(delimiterIdx[3]) + PLUS_ONE_AFTER_DELIMITER, size_t(delimiterIdx[4]) - delimiterIdx[3] - MINUS_ONE_BEFORE_DELIMITER));
        cdrProcessedData.push_back(cdr.substr(size_t(delimiterIdx[6]) + PLUS_ONE_AFTER_DELIMITER, size_t(delimiterIdx[7]) - delimiterIdx[6] - MINUS_ONE_BEFORE_DELIMITER));
        cdrProcessedData.push_back(cdr.substr(size_t(delimiterIdx[7]) + PLUS_ONE_AFTER_DELIMITER, size_t(delimiterIdx[8]) - delimiterIdx[7] - MINUS_ONE_BEFORE_DELIMITER));
        cdrProcessedData.push_back(cdr.substr(size_t(delimiterIdx[8]) + PLUS_ONE_AFTER_DELIMITER, size_t(delimiterIdx[9]) - delimiterIdx[8] - MINUS_ONE_BEFORE_DELIMITER));
        cdrProcessedData.push_back(cdr.substr(size_t(delimiterIdx[9]) + PLUS_ONE_AFTER_DELIMITER));
        
        std::string voiceOut = "0";
        std::string voiceIn = "0";
        std::string smsOut = "0";
        std::string smsIn = "0";
        if (cdrProcessedData.at(1) == "MOC")
        {
            voiceOut = "1";
        }
        else if (cdrProcessedData.at(1) == "MTC")
        {
            voiceIn = "1";
        }
        else if (cdrProcessedData.at(1) == "SMS-MO")
        {
            smsOut = "1";
        }
        else if (cdrProcessedData.at(1) == "SMS-MT")
        {
            smsIn = "1";
        }
        //else if (cdrProcessedData.at(1) == "D")
        //{
        //}
        else if (cdrProcessedData.at(1) == "U")
        {
            voiceOut = "1";
        }
        else if (cdrProcessedData.at(1) == "B" || cdrProcessedData.at(1) == "X")
        {
            voiceOut = "0";
        } //else{}

        DatabaseOperations_mt::SecInCallDetails secPerson{std::stoull(cdrProcessedData.at(6)), {std::stoul(voiceOut) + std::stoul(voiceIn), std::stoul(smsOut) + std::stoul(smsIn)}};

        m_db.InserUserDataToDb(std::stoull(cdrProcessedData.at(0)), std::stoull(cdrProcessedData.at(2)),
            std::stoul(voiceOut),std::stoul(voiceIn), std::stoul(cdrProcessedData.at(4)),
            std::stoul(cdrProcessedData.at(5)), std::stoul(smsOut), std::stoul(smsIn), secPerson);
    }
}

} // namespace experis