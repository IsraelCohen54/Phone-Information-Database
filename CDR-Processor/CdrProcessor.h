#pragma once

#include <vector>
#include <fstream>
#include <string>
#include <iostream>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <mutex>
#include <condition_variable>
#include <cassert>
#include <filesystem>

#include "DataBase.h"
#include "CircularQueue_mt.h"
#include "strings.h"

namespace experis
{

class CdrProcessor_mt
{
	using FolderPath = std::string;
	using FilePath = std::string;
	using CdrLine = std::string;

	static constexpr unsigned short PLUS_ONE_AFTER_DELIMITER = 1;
	static constexpr unsigned short MINUS_ONE_BEFORE_DELIMITER = 1;

public:
	explicit CdrProcessor_mt() = delete;
	explicit CdrProcessor_mt(const FolderPath& a_folderInputPath, const FolderPath& a_afterProcessFolderPath, DatabaseOperations_mt& a_db);
	CdrProcessor_mt(const CdrProcessor_mt& a_other) = delete;
	CdrProcessor_mt& operator=(const CdrProcessor_mt& a_other) = delete;
	~CdrProcessor_mt() = default;

	void GetDirFilesToProcess();
	void EnqueueEachFileToProcess();
	void ProcessCdrsInsideFile();

private:
	DatabaseOperations_mt& m_db;
	FolderPath m_filesProcessDirPath;
	FolderPath m_doneProcessFolderPath;

	CyclicBlockingQueue<FilePath, 100> m_cdrsFilesPath;
	CyclicBlockingQueue<FilePath, 10> m_cdrsToProcess;

	std::mutex m_queueFilesPath;
	std::mutex m_queueCdrsToProcess;
	
	std::condition_variable m_waitForFileTransfer;
	std::condition_variable m_waitForCdrTransfer;
};

} // namespace experis