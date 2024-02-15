#include <iostream>
#include <thread>
#include <chrono>

#include "UITelecommunicationDataQuery.h"
#include "DataBase.h"
#include "CircularQueue_mt.h"
#include "CdrProcessor.h"
#include "StreamInOutDirection_mt.h"

//Telnet:
#include "SimpleNetMT.h"
#include "UrlPathReader.h"

namespace experis
{

// <summary>
/// mostly for debug purpose and to show many input ways to DB
///     inserting new player,
///     adding more values to it,
///     adding for it new contact,
///     than adding new person that was contacted before
/// </summary>
void FillDbHardCoded(DatabaseOperations_mt& a_db)
{
    while(true)
    {
        a_db.InserUserDataToDb(425020528409018, 9720528409042, 2, 3, 4, 5, 6,7
            , DatabaseOperations_mt::SecInCallDetails {9720528409044, {10, 100}});   
        a_db.InserUserDataToDb(425020528409018, 9720528409042, 2, 3, 4, 5, 6,7
            , DatabaseOperations_mt::SecInCallDetails {9720528409044, {10, 100}});   
        a_db.InserUserDataToDb(425020528409018, 9720528409042, 2, 3, 4, 5, 6,7
            , DatabaseOperations_mt::SecInCallDetails {9720528409045, {10, 100}});  
        a_db.InserUserDataToDb(425020528409010, 9720528409045, 4, 43 , 73 , 1, 3 ,2
            , DatabaseOperations_mt::SecInCallDetails {9720528409042, {44, 100}});        
        std::this_thread::sleep_for(std::chrono::seconds(15));
    }
}

void ExecuteInstruction(CyclicBlockingQueue<std::vector<std::string>, 10>& a_queue, DatabaseOperations_mt& a_db, StreamInOutDirection_mt& a_stream)
{
    while(true)
    { 
        std::vector<std::string> instructionHolder;
        a_queue.Dequeue(instructionHolder);
        a_db.QueryDatabaseData(instructionHolder, a_stream);
    }
}

// <summary>
/// using telnet with: set localecho (to see writing inside telnet)
/// open 127.0.0.1 4010 (127.0.0.1 - current computer, else ping needed, 4010 - port number defined)
/// </summary>
void GetInputViaTelnet(DatabaseOperations_mt& a_db, unsigned short a_portNum)
{
    using namespace std::chrono_literals;
    simplenet::SimpleNetMT net{a_portNum};
    bool threadCreatedAlready = false;
	while (true)
	{
		simplenet::SimpleNetMT::Connection c = net.WaitForConnection();
		try 
		{
            std::mutex StreamOutTelnet_Mutex;
            StreamInOutTelnet streamingOutTelnet{c, StreamOutTelnet_Mutex};
            StreamInOutDirection_mt& strmOut{streamingOutTelnet};

			c.Write("Welcome to Telnet! and, (drums.....!)\n");
            std::this_thread::sleep_for(std::chrono::seconds(2));
            c.Write("\n");

            CyclicBlockingQueue<std::vector<std::string>, 10> a_instructionsQueue{};
            if (!threadCreatedAlready)
            {
                std::thread th_execUserInstruction(experis::ExecuteInstruction, std::ref(a_instructionsQueue), std::ref(a_db), std::ref(strmOut));
                th_execUserInstruction.detach();
                threadCreatedAlready = true;
            }
            UI::StartUI(strmOut,  a_instructionsQueue);
		}
		catch (const simplenet::SimpleNetMT::ConnectionClosed&)
		{
			std::cout << "CLOSED\n";
		}
	}
}


} // namespace experis

int main()
{
    using namespace experis;
    
    DatabaseOperations_mt db;
    CdrProcessor_mt cdrProcessor{std::string{"C:\\Users\\hartk\\Desktop\\cdr_Input_dir"}, std::string{"C:\\Users\\hartk\\Desktop\\cdr_Input_dir\\Done}"}, db};

    std::thread th_getDirFilesToProcess(&CdrProcessor_mt::GetDirFilesToProcess, std::ref(cdrProcessor));
    std::thread th_enqueueEachFileToProcess(&CdrProcessor_mt::EnqueueEachFileToProcess, std::ref(cdrProcessor));
    std::thread th_processFilesInsideFolder(&CdrProcessor_mt::ProcessCdrsInsideFile, std::ref(cdrProcessor));

    std::thread th_insertDbData1(&FillDbHardCoded, std::ref(db));
    std::thread th_insertDbData2(&FillDbHardCoded, std::ref(db));

    // open 2 ports for inset data to DB, both can reopened after closed
    std::thread th_TelnetCommunication1(&GetInputViaTelnet, std::ref(db), 4010);
    std::thread th_TelnetCommunication2(&GetInputViaTelnet, std::ref(db), 4011);
   
    std::mutex StreamOut_mtMutex;
    StreamInOutOstream_mt streamingOutOstream {StreamOut_mtMutex, std::cout};
    StreamInOutDirection_mt& strmOut{streamingOutOstream};
    CyclicBlockingQueue<std::vector<std::string>, 10> instructionsQueue;

    // console UI
    std::thread th_getUserInstructions(&UI::StartUI , std::ref(strmOut), std::ref(instructionsQueue));

    // auto execution commands
    std::thread th_execUserInstruction(ExecuteInstruction, std::ref(instructionsQueue), std::ref(db), std::ref(strmOut));

    th_TelnetCommunication1.join();
    th_TelnetCommunication2.join();
    th_processFilesInsideFolder.join();
    assert(!"things happened..., join finished");
    th_enqueueEachFileToProcess.join();
    th_getDirFilesToProcess.join();
    th_getUserInstructions.join();
    th_execUserInstruction.join();
    th_insertDbData1.join();
    th_insertDbData2.join();
}