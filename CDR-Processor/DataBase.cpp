#include <cmath>
#include "DataBase.h"

namespace experis
{

DatabaseOperations_mt::DatabaseOperations_mt()
: m_usersDatabase{}
{
}

// <summary>
/// Inserting user datawhile checking:
/// 1. If user exist in the converting unordered_map from MSISDN to IMSI, add val to it's connected IMSI.
/// 2. If user doesn't exist, because of many to one (many MSISDN to one IMSI) check if IMSI exist, if so, add to it's val, and add MSISDN to the converting unordered_map
/// 3. if even IMSI doesn't exist (a complete new user), add IMSI key and val, update converting unordered map as well.
/// ~ + insert rellevant operator data via funcion to it's own unordered_map ~
/// </summary>
void DatabaseOperations_mt::InserUserDataToDb(const Imsi a_imsi, const Msisdn a_msisdn, const DetailCounter a_voiceOut, const DetailCounter a_voiceIn,
		const DetailCounter a_dataOut, const DetailCounter a_dataIn, const DetailCounter a_smsOut,
		const DetailCounter a_smsIn, const SecInCallDetails a_secondPartyVoiceCallAndSmsVolumeByMsisdn)
{
	{
		std::unique_lock<std::mutex> m_lockInsertOperation(m_subscribersMutex);

		if (m_fromMsisdnToImsi.find(a_msisdn) == m_fromMsisdnToImsi.end())
		{
			// Msisdn does not exist:
			m_fromMsisdnToImsi[a_msisdn] = a_imsi;
			if (m_usersDatabase.find(a_imsi) == m_usersDatabase.end())
			{	// IMSI does not exist:
				m_usersDatabase.insert(std::make_pair(a_imsi, UserUsageDetails_mt{a_voiceOut, a_voiceIn, a_dataOut, a_dataIn, a_smsOut, a_smsIn, a_secondPartyVoiceCallAndSmsVolumeByMsisdn}));
			}
			else
			{
				m_usersDatabase.at(a_imsi).m_voiceOut += a_voiceOut;
				m_usersDatabase.at(a_imsi).m_voiceIn += a_voiceIn;
				m_usersDatabase.at(a_imsi).m_dataOut += a_dataOut;
				m_usersDatabase.at(a_imsi).m_dataIn += a_dataIn;
				m_usersDatabase.at(a_imsi).m_smsOut += a_smsOut;
				m_usersDatabase.at(a_imsi).m_smsIn += a_smsIn;
																 // [...] => if key exist, find and add vals, if not, create and insert:
				m_usersDatabase.at(a_imsi).m_secInCallDetails[a_secondPartyVoiceCallAndSmsVolumeByMsisdn.first].first += a_secondPartyVoiceCallAndSmsVolumeByMsisdn.second.first;
				m_usersDatabase.at(a_imsi).m_secInCallDetails[a_secondPartyVoiceCallAndSmsVolumeByMsisdn.first].second += a_secondPartyVoiceCallAndSmsVolumeByMsisdn.second.second;
			}
		}
		else // Msisdn does exist (=> IMSI exist as well => add vals to IMSI):
		{
			m_usersDatabase.at(a_imsi).m_voiceOut += a_voiceOut;
			m_usersDatabase.at(a_imsi).m_voiceIn += a_voiceIn;
			m_usersDatabase.at(a_imsi).m_dataOut += a_dataOut;
			m_usersDatabase.at(a_imsi).m_dataIn += a_dataIn;
			m_usersDatabase.at(a_imsi).m_smsOut += a_smsOut;
			m_usersDatabase.at(a_imsi).m_smsIn += a_smsIn;
															 // [...] => if key exist, find and add vals, if not, create and insert:
			m_usersDatabase.at(a_imsi).m_secInCallDetails[a_secondPartyVoiceCallAndSmsVolumeByMsisdn.first].first += a_secondPartyVoiceCallAndSmsVolumeByMsisdn.second.first;
			m_usersDatabase.at(a_imsi).m_secInCallDetails[a_secondPartyVoiceCallAndSmsVolumeByMsisdn.first].second += a_secondPartyVoiceCallAndSmsVolumeByMsisdn.second.second;
		}
	} //uniqe lock dtor
	
	InsertDataToOperator(a_imsi, a_voiceOut, a_voiceIn, a_smsOut, a_smsIn);
}

void DatabaseOperations_mt::InsertDataToOperator(const Imsi a_imsi, const DetailCounter a_voiceOut, const DetailCounter a_voiceIn, const DetailCounter a_smsOut, const DetailCounter a_smsIn)
{
	{
		std::unique_lock<std::mutex> lockFromMsisdnToImsi(m_operatorMutex);

		MccMnc mccMnc{MccMnc(a_imsi / 10000000000)}; //get first 5 digits out of 15 digits number (IMSI)

		auto it = m_operatorDataAggregation.find(mccMnc);
		if (it != m_operatorDataAggregation.end())
		{
			it->second.AddOperatorValues(a_voiceOut, a_voiceIn, a_smsOut, a_smsIn);
		}
		else
		{	// a_imsi does not exist:
			m_operatorDataAggregation.insert(std::make_pair(mccMnc, DatabaseOperations_mt::OperatorData_mt{a_voiceOut, a_voiceIn, a_smsOut, a_smsIn}));
		}
	} //uniqe lock dtor
}

void DatabaseOperations_mt::QueryDatabaseData(const std::vector<InstructionType>& a_userProcessedInstruction, StreamInOutDirection_mt& a_stream) const
{
	static constexpr int MSISDN = 1;
	static constexpr int OPERATOR_DATA = 2;
	static constexpr int TWO_PERSON_COMMON_DATA = 3;
	switch (std::stoi(a_userProcessedInstruction.at(0))) //Todo array of commands instead of switch case!!!
	{
	case(MSISDN): //Todo change vector type from string! it does not longer contain msisdn
	{
		{
			std::unique_lock<std::mutex> billingDataLock (m_subscribersMutex);

			Msisdn msisdn = std::stoull(a_userProcessedInstruction.at(1));
			auto it = m_fromMsisdnToImsi.find(msisdn);
			if (it != m_fromMsisdnToImsi.end())
			{
			    Imsi imsi = it->second;
			    m_usersDatabase.at(imsi).GetBillingData(msisdn ,a_stream);
			}
			else
			{
			    a_stream << "You might have a typo in your MSISDN number,\n as " << msisdn << " currently doesn't exist as an MSISDN inside the database.\n\n";
			}
		} // uniqe lock dtor
		break;
	}
	case(OPERATOR_DATA):
	{
		MccMnc mccMnc = std::stoi(a_userProcessedInstruction.at(1));
		{
			std::unique_lock<std::mutex> operatorDataLock (m_operatorMutex);

			auto it = m_operatorDataAggregation.find(mccMnc);
			if (it != m_operatorDataAggregation.end())
			{
				it->second.GetOperatorData(a_stream);
			}
			else
			{
			    a_stream << "You might have a typo in your MCCMNC number,\n as " << mccMnc << " currently doesn't exist as an MCCMNC inside the database.\n\n";
			}
		} // uniqe lock dtor
		break;
	}
	case(TWO_PERSON_COMMON_DATA): //Todo a *2 ways check*, as a CDR of one way might arrived later, or not updated as the sec person data (which might have inserted already to DB)
	{
		Msisdn msiSdnFirstPerson{std::stoull(a_userProcessedInstruction.at(1))};
		Msisdn msiSdnSecondPerson{std::stoull(a_userProcessedInstruction.at(2))};

		{
			std::unique_lock<std::mutex> billingDataLock (m_subscribersMutex);
			auto itFindImsi = m_fromMsisdnToImsi.find(msiSdnFirstPerson);
			if (itFindImsi != m_fromMsisdnToImsi.end())
			{
				auto itSecCallerData = m_usersDatabase.find(itFindImsi->second);
				if (itSecCallerData != m_usersDatabase.end())
				{
					itSecCallerData.operator*().second.GetCommonDataWithOnePerson(msiSdnSecondPerson ,a_stream);
				}
				else
				{
					a_stream << "person 2 is currently unknown\n";
				}
			}
			else
			{
				a_stream << "person 1 is currently unknown\n";
			}
		} // uniqe lock dtor
		break;
	}
	case(4): // TODO
		a_stream << "Currently not implemented due lack of time\n";
		break;
	default:
		assert(!"at GetDataPrinted, after validate input");
		break;
	}
}

// ~~~ inner class UserUsageDetails_mt implementations ~~~

DatabaseOperations_mt::UserUsageDetails_mt::UserUsageDetails_mt(const DataType a_voiceOut, const DataType a_voiceIn ,const DataType a_dataOut,
		const DataType a_dataIn, const DataType a_smsOut, const DataType a_smsIn, const SecInCallDetails a_secInCallDetail)
: m_voiceOut{a_voiceOut}
, m_voiceIn{a_voiceIn}
, m_dataOut{a_dataOut}
, m_dataIn{a_dataIn}
, m_smsOut{a_smsOut}
, m_smsIn{a_smsIn}
, m_secInCallDetails{} // init vals outside MIL (member init' list)
{
	m_secInCallDetails[a_secInCallDetail.first].first += a_secInCallDetail.second.first;
	m_secInCallDetails[a_secInCallDetail.first].second += a_secInCallDetail.second.second;
}

DatabaseOperations_mt::UserUsageDetails_mt& DatabaseOperations_mt::UserUsageDetails_mt::operator=(const UserUsageDetails_mt& a_other)
{
	if (this == &a_other)
    {
        return *this; // => not self-assignment
    }
    m_voiceOut = a_other.m_voiceOut;
    m_voiceIn = a_other.m_voiceIn;
    m_dataOut = a_other.m_dataOut;
    m_dataIn = a_other.m_dataIn;
    m_smsOut = a_other.m_smsOut;
    m_smsIn = a_other.m_smsIn;
    m_secInCallDetails = a_other.m_secInCallDetails;

    return *this;
}

void DatabaseOperations_mt::UserUsageDetails_mt::GetBillingData(const Msisdn a_msisdn, StreamInOutDirection_mt& a_stream) const noexcept // Todo should have send ovject to class that would print it
{ 
	// locked stream:
	a_stream << "\nmsisdn: " << a_msisdn << "\nvoice-out: " << m_voiceOut << "\nvoice-in: " << m_voiceIn << "\ndata-out: " << m_dataOut << "\ndata-in: " << m_dataIn << "\nsms-out: " << m_smsOut << "\nsms-in: " << m_smsIn << "\n\n";
}

void DatabaseOperations_mt::UserUsageDetails_mt::GetCommonDataWithOnePerson(const Msisdn msiSdnSecondPerson, StreamInOutDirection_mt& a_stream) const
{
	auto it = m_secInCallDetails.find(msiSdnSecondPerson);
	if (it != m_secInCallDetails.end())
	{	// locked stream:
		a_stream << "\nTotal voice calls volume excanched: " << it->second.first << "\nTotal smses volume excanched: " << it->second.second << "\n\n";
	}
	else
	{
		a_stream << "The second person's data doesn't exist. It might not have been uploaded yet.\n";
	}
}

// ~~~ inner class OperatorData_mt implementations ~~~
DatabaseOperations_mt::OperatorData_mt::OperatorData_mt(const DataType a_voiceOut, const DataType a_voiceIn, const DataType a_smsOut, const DataType a_smsIn)
: m_voiceOut{a_voiceOut}
, m_voiceIn{a_voiceIn}
, m_smsOut{a_smsOut}
, m_smsIn{a_smsIn}
{
}

DatabaseOperations_mt::OperatorData_mt::OperatorData_mt(const OperatorData_mt& a_other)
: m_voiceOut{a_other.m_voiceOut}
, m_voiceIn{a_other.m_voiceIn}
, m_smsOut{a_other.m_smsOut}
, m_smsIn{a_other.m_smsIn}
{
}

void DatabaseOperations_mt::OperatorData_mt::GetOperatorData(StreamInOutDirection_mt& a_stream) const noexcept
{	// locked stream
	a_stream << "\nm_voiceOut: " << m_voiceOut << "\nm_voiceIn: " << m_voiceIn << "\nm_smsOut: " << m_smsOut << "\nm_smsIn: " << m_smsIn << "\n\n";
}

void DatabaseOperations_mt::OperatorData_mt::AddOperatorValues(const DetailCounter a_voiceOut, const DetailCounter a_voiceIn, const DetailCounter a_smsOut, const DetailCounter a_smsIn)
{
	m_voiceOut += a_voiceOut;
	m_voiceIn += a_voiceIn;
	m_smsOut += a_smsOut;
	m_smsIn += a_smsIn;
}

/*
void DatabaseOperations_mt::UserUsageDetails_mt::operator+=(UserUsageDetails_mt a_usrDetails)
{
	this->m_dataIn += a_usrDetails.m_dataIn;
	this->m_dataOut += a_usrDetails.m_dataOut;
	this->m_smsIn += a_usrDetails.m_smsIn;
	this->m_smsOut += a_usrDetails.m_smsOut;
	this->m_voiceIn += a_usrDetails.m_voiceIn;
	this->m_voiceOut += a_usrDetails.m_voiceOut;
	//this->m_secInCallDetails += a_usrDetails.m_secInCallDetails;
}*/

} //namespace experis
