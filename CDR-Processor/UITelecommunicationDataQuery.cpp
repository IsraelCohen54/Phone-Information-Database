#include <array>
#include "UITelecommunicationDataQuery.h"

namespace experis
{

namespace UI //static class
{

// Todo min phone num for IMSI and msisdn...
static constexpr unsigned long long MAX_PHONE_NUM = 999999999999999;
static constexpr unsigned int MAX_OPERATOR_NUM = 99999;
static constexpr unsigned int MIN_OPERATOR_NUM = 10000;
static constexpr unsigned short MIN_DELIMITER_NUM = 1;
static constexpr unsigned short MAX_DELIMITER_NUM = 2;
static constexpr unsigned short EXIT = 5;
static std::array<std::string, 5> INSTRUCTIONS {
        "",
        "\nFor billing data, please write msisdn/MSISDN (e.g. msisdn/9720528409042) \n",
        "\nFor usage information of one operator, please write operator/MCCMNC? (e.g. operator/42502) \n",
        "\nFor Produce a list of all subscribers who were in direct contact with a given subscriber,\nidentified by MSISDN, please write: link/first-party/second-party (e.g. link/9720528409042/9720528409045) \n",
        "\nFor a list of all subscribers along a path between two given subscribers \nidentified by their MSISDN, please write: link/first-party/second-party (e.g. link/9720528409042/496221540) \n"
    };

using DelimiterType = unsigned short;
using InstructionsType = std::string;
using UserChoice = unsigned short;
using StreamType = std::ostream;

void FillVectorWithInstructions(const std::vector<DelimiterType>& a_delimiterIdx,
    std::vector<InstructionsType>& a_processedInstructionData, const InstructionsType& a_unproccessedInstruction) 
{
    //assert(a_delimiterIdx.at(0) < 10000); // Todo check if cast could be avoided using assert of unsigned short range (b.t.w., assert can be used with complier analyzer)
    //assert(a_delimiterIdx.at(0) > 0);

    InstructionsType firstPart{a_unproccessedInstruction.substr(0, a_delimiterIdx.at(0))};
    a_processedInstructionData.push_back(firstPart);

    InstructionsType secPart{a_unproccessedInstruction.substr(size_t(a_delimiterIdx.at(0)) + 1)};  

    InstructionsType thirdPart{};
    if (a_delimiterIdx.size() == 2)
    {   //(tricks to meybe avoid size_t casting - vector, indx, len \substr + assert that i'm inside short range (assert >= 0 < 10K) explain to compiler)
        secPart = a_unproccessedInstruction.substr(size_t(a_delimiterIdx.at(0)) + 1, size_t(a_delimiterIdx.at(1)) - size_t(a_delimiterIdx.at(0)) - 1); //TODO validate debbuging
        thirdPart = a_unproccessedInstruction.substr(size_t(a_delimiterIdx.at(1)) + 1);
    }
    a_processedInstructionData.push_back(secPart);
    a_processedInstructionData.push_back(thirdPart);
}

void PrintInstructionPerUserChoice(const UserChoice a_usrChoice, StreamInOutDirection_mt& a_stream)
{
    assert(a_usrChoice > 0 && a_usrChoice <= 4);
    a_stream << INSTRUCTIONS[a_usrChoice];
}

static void TellUserInputWasntOk(std::string& untrust_userInput, StreamInOutDirection_mt& a_stream)
{
    if (untrust_userInput != "")
    {
	    a_stream << "Please try again, you have entered: " << untrust_userInput << " instead of a digit from 1 to 5\n";
    }
    else
    {
        a_stream << "Please try again, you have entered: an empty string instead of a digit from 1 to 5\n";
    }
    a_stream.GetLine( untrust_userInput);
}

bool UserInstructionsValidation(const InstructionsType& a_untrust_instruction, const UserChoice a_usrChoice) 
{
    std::vector<DelimiterType> delimiterIdx{};
    const char delimiterType = '/';
    GetDelimiterIdx(delimiterIdx, delimiterType, a_untrust_instruction);
    if ((delimiterIdx.size() != MAX_DELIMITER_NUM && a_usrChoice >= 3) || (delimiterIdx.size() != MIN_DELIMITER_NUM && a_usrChoice < 3)) // validate str/num/num or str/num
    {
        return false;
    } //else{}
    
    std::vector<InstructionsType> instructionData{};
    FillVectorWithInstructions(delimiterIdx, instructionData, a_untrust_instruction);

                         // Todo shorten validation rellevant to all... DRY...
                         // Todo upper minimum number limit, as it's not zero
    switch (a_usrChoice) //Todo consider support of CAPITAL LETTERS for all options + max number & minimum number check for all, range of operator number and more
    {
    case 1:
        return ((instructionData.at(0) == "msisdn") && OnlyDigits(instructionData.at(1)) && MAX_PHONE_NUM >= std::stoull(instructionData.at(1)));
    case 2:
        return ((instructionData.at(0) == "operator") && OnlyDigits(instructionData.at(1)) && (std::stoi(instructionData.at(1)) <= MAX_OPERATOR_NUM && std::stoi(instructionData.at(1)) >= MIN_OPERATOR_NUM));
    case 3:
        return ((instructionData.at(0) == "link") && OnlyDigits(instructionData.at(1)) && OnlyDigits(instructionData.at(2)) && std::stoull(instructionData.at(2)) > 0 && (std::stoull(instructionData.at(1)) <= MAX_PHONE_NUM) && (std::stoull(instructionData.at(2)) <= MAX_PHONE_NUM) && std::stoull(instructionData.at(1)) > 0);
    case 4: //Todo add validation..
        return ((instructionData.at(0) == "link") && OnlyDigits(instructionData.at(1)) && OnlyDigits(instructionData.at(2))); //same as 3
    default:
        assert(!"I shouldn't be here after validation! at InstructionsValidation");
        break;
    }
    return false; // would happen only without assert... IMO
}

void StartUI(StreamInOutDirection_mt& a_stream, CyclicBlockingQueue<std::vector<std::string>, 10>& a_queue)
{
	a_stream << "Welcome to the Telecommunication data center!\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\nPlease choose:\n\n";
    try // might be a 'no memory alloc' for string creation (with substring), catch inside thread to know it has stopped...
    {
        while(true)
        {
            a_stream << " 1 for billing data\n 2 for operator data\n 3 for all user contacted by a user\n 4 for list of calls path between to user if such exist\n 5 for exit\n\n";
	        std::string untrust_userInput{};
	        a_stream.GetLine(untrust_userInput);
	        while (!OnlyDigits(untrust_userInput) || !OneToFive(untrust_userInput) || untrust_userInput == "")
	        {
                TellUserInputWasntOk(untrust_userInput, a_stream);
	        }
	        UserChoice userInputOption{UserChoice(std::stoi(untrust_userInput))};
            if (userInputOption == EXIT)
            {
                break;
            }
            PrintInstructionPerUserChoice(userInputOption, a_stream);

            a_stream.GetLine( untrust_userInput);
            while(!UserInstructionsValidation(untrust_userInput, userInputOption))
            {
                a_stream << "Please try again, you have entered: " << untrust_userInput << " \n";
                a_stream.GetLine( untrust_userInput);
            }
            std::string userInstruction = untrust_userInput;

            // Todo work without string, no needed after change
            // Todo shorten it by much if time would allow... (=> func that would fill vec, and call on the way to validate, if validate doesn't pass - try again...)
            std::vector<DelimiterType> delimiterIdx{};
            GetDelimiterIdx(delimiterIdx, '/', userInstruction);
            std::vector<InstructionsType> instructionData{};
            FillVectorWithInstructions(delimiterIdx, instructionData, userInstruction);
            
            instructionData.at(0) = std::to_string(userInputOption);
            a_queue.Enqueue(instructionData);

            // instructionData.WaitUntilFinishedExecuting(); //Todo instructionData pair of data
                                                             // + mutex and conditional variable that would case a wait until execute, need a new class for that
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            
            a_stream << "Would you like to know anything else? please choose:\n\n";
        }
    }
    catch (const std::exception& a_exc)
    {
        a_stream << a_exc.what();
    }
}

} //namespace UI

} // namespace experis
