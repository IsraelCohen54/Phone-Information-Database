#pragma once
#include <iostream>
#include <unordered_map>
#include <vector>
#include <string>
#include <cassert>
#include <mutex>
#include <condition_variable>

#include "StreamInOutDirection_mt.h"

namespace experis
{

class DatabaseOperations_mt
{
	class UserUsageDetails_mt;
	class OperatorData_mt;

public:
	using Imsi = unsigned long long;   // Who's actually paying for the number, num up to 15 digits
	using Msisdn = unsigned long long; // Need up to 15 digits
	using DetailCounter = unsigned int;
	using VoiceCall = DetailCounter;
	using SmsVolume = DetailCounter;
	using SecInCallDetails = std::pair<Msisdn, std::pair<VoiceCall, SmsVolume>>;
	using InstructionType = std::string;
	using StreamType = std::ostream;
	using MccMnc = unsigned int; // should be exactly 5 digits: First 3 digits represent the country, next 2 digits represents the mobile network operator.
	using UserOption = unsigned short;
	
	explicit DatabaseOperations_mt();
	explicit DatabaseOperations_mt(const DatabaseOperations_mt& a_other) = delete;
	DatabaseOperations_mt& operator=(const DatabaseOperations_mt& a_other) = delete;
	~DatabaseOperations_mt() = default;
	
	void InserUserDataToDb(const Imsi a_imsi, const Msisdn a_msisdn, const DetailCounter a_voiceOut, const DetailCounter a_voiceIn,
		const DetailCounter a_dataOut, const DetailCounter a_dataIn, const DetailCounter a_smsOut,
		const DetailCounter a_smsIn, const SecInCallDetails a_secondPartyVoiceCallAndSmsVolumeByMsisdn);
	void QueryDatabaseData(const std::vector<InstructionType>& a_userProcessedInstruction, StreamInOutDirection_mt& a_stream) const;
	
private:
	using DataType = size_t;

	std::unordered_map<Msisdn, Imsi> m_fromMsisdnToImsi; 
	std::unordered_map<Imsi, UserUsageDetails_mt> m_usersDatabase; 
	std::unordered_map<MccMnc, OperatorData_mt> m_operatorDataAggregation;
	
	void InsertDataToOperator(const Imsi a_imsi, const DetailCounter a_voiceOut, const DetailCounter a_voiceIn, const DetailCounter a_smsOut, const DetailCounter a_smsIn);

	mutable std::mutex m_subscribersMutex;
	mutable std::mutex m_operatorMutex;
};

class DatabaseOperations_mt::UserUsageDetails_mt
{
	friend class DatabaseOperations_mt; // instead of += operator, as doing it with += didn't worked with field of unordered map, as needed more data for += operator than only 2 (1) inputs (this + 1)

public:
	explicit UserUsageDetails_mt() = delete;
	explicit UserUsageDetails_mt(const DataType a_voiceOut, const DataType a_voiceIn ,const DataType a_dataOut,
		const DataType a_dataIn, const DataType a_smsOut, const DataType a_smsIn, const SecInCallDetails a_secInCallDetails);
	UserUsageDetails_mt(const UserUsageDetails_mt& a_other) = default;
	UserUsageDetails_mt& operator=(const UserUsageDetails_mt& a_other); // for inserting to unordered_map r-value type
	~UserUsageDetails_mt() = default;

	void GetBillingData(const Msisdn a_msisdn, StreamInOutDirection_mt& a_stream) const noexcept; // mt safe (private class, used once inside uniqe lock lock)
	void GetCommonDataWithOnePerson(const Msisdn msiSdnSecondPerson, StreamInOutDirection_mt& a_stream) const; // mt safe (private class, used once inside uniqe lock lock)
	
private:
    DataType m_voiceOut;
    DataType m_voiceIn;
    DataType m_dataOut;
    DataType m_dataIn;
    DataType m_smsOut;
    DataType m_smsIn;
	std::unordered_map<Msisdn, std::pair<VoiceCall, SmsVolume>> m_secInCallDetails;
};

class DatabaseOperations_mt::OperatorData_mt
{
public:
	explicit OperatorData_mt() = delete;
	explicit OperatorData_mt(const DataType a_voiceOut, const DataType a_voiceIn, const DataType a_smsOut, const DataType a_smsIn);
	OperatorData_mt(const OperatorData_mt& a_other);
											   
	OperatorData_mt operator=(const OperatorData_mt& a_other) = delete;
	~OperatorData_mt() = default;
	
	void GetOperatorData(StreamInOutDirection_mt& a_stream) const noexcept; //mt safe - inside lock + private class
	void AddOperatorValues(const DetailCounter a_voiceOut, const DetailCounter a_voiceIn, const DetailCounter a_smsOut, const DetailCounter a_smsIn); //mt safe - inside lock + private class

private:
	DataType m_voiceOut;
	DataType m_voiceIn;
	DataType m_smsOut;
	DataType m_smsIn;
};

} // experis namespace