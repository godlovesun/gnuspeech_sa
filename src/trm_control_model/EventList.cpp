/***************************************************************************
 *  Copyright 1991, 1992, 1993, 1994, 1995, 1996, 2001, 2002               *
 *    David R. Hill, Leonard Manzara, Craig Schock                         *
 *                                                                         *
 *  This program is free software: you can redistribute it and/or modify   *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation, either version 3 of the License, or      *
 *  (at your option) any later version.                                    *
 *                                                                         *
 *  This program is distributed in the hope that it will be useful,        *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *  GNU General Public License for more details.                           *
 *                                                                         *
 *  You should have received a copy of the GNU General Public License      *
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 ***************************************************************************/
// 2014-09
// This file was copied from Gnuspeech and modified by Marcelo Y. Matuda.

#include "EventList.h"

//#include <cmath> /* NAN */
#include <cstdlib> /* random */
#include <cstring>
#include <limits> /* std::numeric_limits<double>::infinity() */
#include <sstream>
#include <vector>

#include "Log.h"



#define DIPHONE 2
#define TRIPHONE 3
#define TETRAPHONE 4

#define INTONATION_CONFIG_FILE_NAME "/intonation"

#define INVALID_EVENT_VALUE std::numeric_limits<double>::infinity()



namespace GS {
namespace TRMControlModel {

Event::Event() : time(0), flag(0)
{
	for (int i = 0; i < EVENTS_SIZE; ++i) {
		events[i] = INVALID_EVENT_VALUE;
	}
}

EventList::EventList(const char* configDirPath, Model& model)
		: model_(model)
		, macroFlag_(0)
		, microFlag_(0)
		, driftFlag_(0)
		, smoothIntonation_(1)
		, globalTempo_(1.0)
		, tgParameters_(5)
{
	setUp();

	radiusMultiply_ = 1.0;

	list_.reserve(128);

	initToneGroups(configDirPath);
}

EventList::~EventList()
{
}

void
EventList::setUp()
{
	list_.clear();

	zeroRef_ = 0;
	zeroIndex_ = 0;
	duration_ = 0;
	timeQuantization_ = 4;

	multiplier_ = 1.0;
	intonParms_ = nullptr;

	phoneData_.clear();
	phoneData_.push_back(PhoneData());
	phoneTempo_.clear();
	phoneTempo_.push_back(1.0);
	currentPhone_ = 0;

	feet_.clear();
	feet_.push_back(Foot());
	currentFoot_ = 0;

	toneGroups_.clear();
	toneGroups_.push_back(ToneGroup());
	currentToneGroup_ = 0;

	ruleData_.clear();
	ruleData_.push_back(RuleData());
	currentRule_ = 0;
}

void
EventList::setUpDriftGenerator(float deviation, float sampleRate, float lowpassCutoff)
{
	driftGenerator_.setUp(deviation, sampleRate, lowpassCutoff);
}

void
EventList::parseGroups(int index, int number, FILE* fp)
{
	char line[256];
	tgParameters_[index].resize(10 * number);
	for (int i = 0; i < number; ++i) {
		fgets(line, 256, fp);
		float* temp = &tgParameters_[index][i * 10];
		sscanf(line, " %f %f %f %f %f %f %f %f %f %f",
			&temp[0], &temp[1], &temp[2], &temp[3], &temp[4],
			&temp[5], &temp[6], &temp[7], &temp[8], &temp[9]);
	}
}

void
EventList::initToneGroups(const char* configDirPath)
{
	FILE* fp;
	char line[256];
	int count = 0;

	std::ostringstream path;
	path << configDirPath << INTONATION_CONFIG_FILE_NAME;
	fp = fopen(path.str().c_str(), "r");
	if (fp == NULL) {
		THROW_EXCEPTION(IOException, "Could not open the file " << path.str().c_str() << '.');
	}
	while (fgets(line, 256, fp) != NULL) {
		if ((line[0] == '#') || (line[0] == ' ')) {
			// Skip.
		} else if (strncmp(line, "TG", 2) == 0) {
			sscanf(&line[2], " %d", &tgCount_[count]);
			parseGroups(count, tgCount_[count], fp);
			count++;
		} else if (strncmp(line, "RANDOM", 6) == 0) {
			sscanf(&line[6], " %f", &intonationRandom_);
		}
	}
	fclose(fp);

	if (Log::debugEnabled) {
		printToneGroups();
	}
}

void
EventList::printToneGroups()
{
	printf("===== Intonation configuration:\n");
	printf("Intonation random = %f\n", intonationRandom_);
	printf("Tone groups: %d %d %d %d %d\n", tgCount_[0], tgCount_[1], tgCount_[2], tgCount_[3], tgCount_[4]);

	for (int i = 0; i < 5; i++) {
		float* temp = &tgParameters_[i][0];
		printf("Temp [%d] = %p\n", i, temp);
		int j = 0;
		for (int k = 0; k < tgCount_[i]; k++) {
			printf("%f %f %f %f %f %f %f %f %f %f\n",
				temp[j]  , temp[j+1], temp[j+2], temp[j+3], temp[j+4],
				temp[j+5], temp[j+6], temp[j+7], temp[j+8], temp[j+9]);
			j += 10;
		}
	}
}

double
EventList::getBeatAtIndex(int ruleIndex) const
{
	if (ruleIndex > currentRule_) {
		return 0.0;
	} else {
		return ruleData_[ruleIndex].beat;
	}
}

void
EventList::newPhoneWithObject(const Phone& p)
{
	if (phoneData_[currentPhone_].phone) {
		phoneData_.push_back(PhoneData());
		phoneTempo_.push_back(1.0);
		currentPhone_++;
	}
	phoneTempo_[currentPhone_] = 1.0;
	phoneData_[currentPhone_].ruleTempo = 1.0;
	phoneData_[currentPhone_].phone = &p;
}

void
EventList::replaceCurrentPhoneWith(const Phone& p)
{
	if (phoneData_[currentPhone_].phone) {
		phoneData_[currentPhone_].phone = &p;
	} else {
		phoneData_[currentPhone_ - 1].phone = &p;
	}
}

void
EventList::setCurrentToneGroupType(int type)
{
	toneGroups_[currentToneGroup_].type = type;
}

void
EventList::newFoot()
{
	if (currentPhone_ == 0) {
		return;
	}

	feet_[currentFoot_++].end = currentPhone_;
	newPhone();

	feet_.push_back(Foot());
	feet_[currentFoot_].start = currentPhone_;
	feet_[currentFoot_].end = -1;
	feet_[currentFoot_].tempo = 1.0;
}

void
EventList::setCurrentFootMarked()
{
	feet_[currentFoot_].marked = 1;
}

void
EventList::setCurrentFootLast()
{
	feet_[currentFoot_].last = 1;
}

void
EventList::setCurrentFootTempo(double tempo)
{
	feet_[currentFoot_].tempo = tempo;
}

void
EventList::setCurrentPhoneTempo(double tempo)
{
	phoneTempo_[currentPhone_] = tempo;
}

void
EventList::setCurrentPhoneRuleTempo(float tempo)
{
	phoneData_[currentPhone_].ruleTempo = tempo;
}

void
EventList::newToneGroup()
{
	if (currentFoot_ == 0) {
		return;
	}

	toneGroups_[currentToneGroup_++].endFoot = currentFoot_;
	newFoot();

	toneGroups_.push_back(ToneGroup());
	toneGroups_[currentToneGroup_].startFoot = currentFoot_;
	toneGroups_[currentToneGroup_].endFoot = -1;
}

void
EventList::newPhone()
{
	if (phoneData_[currentPhone_].phone) {
		phoneData_.push_back(PhoneData());
		phoneTempo_.push_back(1.0);
		currentPhone_++;
	}
	phoneTempo_[currentPhone_] = 1.0;
}

void
EventList::setCurrentPhoneSyllable()
{
	phoneData_[currentPhone_].syllable = 1;
}

Event*
EventList::insertEvent(int number, double time, double value)
{
	time = time * multiplier_;
	if (time < 0.0) {
		return nullptr;
	}
	if (time > (double) (duration_ + timeQuantization_)) {
		return nullptr;
	}

	int tempTime = zeroRef_ + (int) time;
	tempTime = (tempTime >> 2) << 2;
	//if ((tempTime % timeQuantization) != 0) {
	//	tempTime++;
	//}

	if (list_.empty()) {
		std::unique_ptr<Event> tempEvent(new Event());
		tempEvent->time = tempTime;
		if (number >= 0) {
			tempEvent->setValue(value, number);
		}
		list_.push_back(std::move(tempEvent));
		return list_.back().get();
	}

	int i;
	for (i = list_.size() - 1; i >= zeroIndex_; i--) {
		if (list_[i]->time == tempTime) {
			if (number >= 0) {
				list_[i]->setValue(value, number);
			}
			return list_[i].get();
		}
		if (list_[i]->time < tempTime) {
			std::unique_ptr<Event> tempEvent(new Event());
			tempEvent->time = tempTime;
			if (number >= 0) {
				tempEvent->setValue(value, number);
			}
			list_.insert(list_.begin() + (i + 1), std::move(tempEvent));
			return list_[i + 1].get();
		}
	}

	std::unique_ptr<Event> tempEvent(new Event());
	tempEvent->time = tempTime;
	if (number >= 0) {
		tempEvent->setValue(value, number);
	}
	list_.insert(list_.begin() + (i + 1), std::move(tempEvent));
	return list_[i + 1].get();
}

void
EventList::setZeroRef(int newValue)
{
	zeroRef_ = newValue;
	zeroIndex_ = 0;

	if (list_.empty()) {
		return;
	}

	for (int i = list_.size() - 1; i >= 0; i--) {
		if (list_[i]->time < newValue) {
			zeroIndex_ = i;
			return;
		}
	}
}

double
EventList::createSlopeRatioEvents(
		const Transition::SlopeRatio& slopeRatio,
		double baseline, double parameterDelta, double min, double max, int eventIndex)
{
	int i, numSlopes;
	double temp = 0.0, temp1 = 0.0, intervalTime = 0.0, sum = 0.0, factor = 0.0;
	double baseTime = 0.0, endTime = 0.0, totalTime = 0.0, delta = 0.0;
	double startValue;
	double pointTime, pointValue;

	Transition::getPointData(*slopeRatio.pointList.front(), model_, pointTime, pointValue);
	baseTime = pointTime;
	startValue = pointValue;

	Transition::getPointData(*slopeRatio.pointList.back(), model_, pointTime, pointValue);
	endTime = pointTime;
	delta = pointValue - startValue;

	temp = slopeRatio.totalSlopeUnits();
	totalTime = endTime - baseTime;

	numSlopes = slopeRatio.slopeList.size();
	std::vector<double> newPointValues(numSlopes - 1);
	for (i = 1; i < numSlopes + 1; i++) {
		temp1 = slopeRatio.slopeList[i - 1]->slope / temp; /* Calculate normal slope */

		/* Calculate time interval */
		intervalTime = Transition::getPointTime(*slopeRatio.pointList[i], model_)
				- Transition::getPointTime(*slopeRatio.pointList[i - 1], model_);

		/* Apply interval percentage to slope */
		temp1 = temp1 * (intervalTime / totalTime);

		/* Multiply by delta and add to last point */
		temp1 = temp1 * delta;
		sum += temp1;

		if (i < numSlopes) {
			newPointValues[i - 1] = temp1;
		}
	}
	factor = delta / sum;
	temp = startValue;

	double value = 0.0;
	for (i = 0; i < slopeRatio.pointList.size(); i++) {
		const Transition::Point& point = *slopeRatio.pointList[i];

		if (i >= 1 && i < slopeRatio.pointList.size() - 1) {
			pointTime = Transition::getPointTime(point, model_);

			pointValue = newPointValues[i - 1];
			pointValue *= factor;
			pointValue += temp;
			temp = pointValue;
		} else {
			Transition::getPointData(point, model_, pointTime, pointValue);
		}

		value = baseline + ((pointValue / 100.0) * parameterDelta);
		if (value < min) {
			value = min;
		} else if (value > max) {
			value = max;
		}
		if (!point.isPhantom) {
			insertEvent(eventIndex, pointTime, value);
		}
	}

	return value;
}

// It is assumed that phoneList.size() >= 2.
void
EventList::applyRule(const Rule& rule, const PhoneSequence& phoneList, const double* tempos, int phoneIndex)
{
	int i, j, cont;
	int currentType;
	double currentValueDelta, value, lastValue;
	double ruleSymbols[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
	double tempTime;
	double targets[4];
	Event* tempEvent = nullptr;

	rule.evaluateExpressionSymbols(tempos, phoneList, model_, ruleSymbols);

	multiplier_ = 1.0 / (double) (phoneData_[phoneIndex].ruleTempo);

	int type = rule.numberOfExpressions();
	setDuration((int) (ruleSymbols[0] * multiplier_));

	ruleData_[currentRule_].firstPhone = phoneIndex;
	ruleData_[currentRule_].lastPhone = phoneIndex + (type - 1);
	ruleData_[currentRule_].beat = (ruleSymbols[1] * multiplier_) + (double) zeroRef_;
	ruleData_[currentRule_++].duration = ruleSymbols[0] * multiplier_;
	ruleData_.push_back(RuleData());

	switch (type) {
	/* Note: Case 4 should execute all of the below, case 3 the last two */
	case 4:
		if (phoneList.size() == 4) {
			phoneData_[phoneIndex + 3].onset = (double) zeroRef_ + ruleSymbols[1];
			tempEvent = insertEvent(-1, ruleSymbols[3], 0.0);
			if (tempEvent) tempEvent->flag = 1;
		}
	case 3:
		if (phoneList.size() >= 3) {
			phoneData_[phoneIndex + 2].onset = (double) zeroRef_ + ruleSymbols[1];
			tempEvent = insertEvent(-1, ruleSymbols[2], 0.0);
			if (tempEvent) tempEvent->flag = 1;
		}
	case 2:
		phoneData_[phoneIndex + 1].onset = (double) zeroRef_ + ruleSymbols[1];
		tempEvent = insertEvent(-1, 0.0, 0.0);
		if (tempEvent) tempEvent->flag = 1;
		break;
	}

	//tempTargets = (List *) [rule parameterList];

	/* Loop through the parameters */
	for (i = 0; i < Parameter::NUM_PARAMETERS; i++) {
		/* Get actual parameter target values */
		targets[0] = phoneList[0]->getParameterValue(i);
		targets[1] = phoneList[1]->getParameterValue(i);
		targets[2] = (phoneList.size() >= 3) ? phoneList[2]->getParameterValue(i) : 0.0;
		targets[3] = (phoneList.size() == 4) ? phoneList[3]->getParameterValue(i) : 0.0;

		/* Optimization, Don't calculate if no changes occur */
		cont = 1;
		switch (type) {
		case DIPHONE:
			if (targets[0] == targets[1]) {
				cont = 0;
			}
			break;
		case TRIPHONE:
			if ((targets[0] == targets[1]) && (targets[0] == targets[2])) {
				cont = 0;
			}
			break;
		case TETRAPHONE:
			if ((targets[0] == targets[1]) && (targets[0] == targets[2]) && (targets[0] == targets[3])) {
				cont = 0;
			}
			break;
		}

		insertEvent(i, 0.0, targets[0]);

		if (cont) {
			currentType = DIPHONE;
			currentValueDelta = targets[1] - targets[0];
			lastValue = targets[0];
			//lastValue = 0.0;

			const std::string& paramTransition = rule.getParamProfileTransition(i);
			const Transition* trans = model_.findTransition(paramTransition); //TODO: check not null

			/* Apply lists to parameter */
			for (j = 0; j < trans->pointOrSlopeList().size(); ++j) {
				const Transition::PointOrSlope& pointOrSlope = *trans->pointOrSlopeList()[j];
				if (pointOrSlope.isSlopeRatio()) {
					const auto& slopeRatio = dynamic_cast<const Transition::SlopeRatio&>(pointOrSlope);

					if (slopeRatio.pointList[0]->type != currentType) { //TODO: check pointList.size() > 0
						currentType = slopeRatio.pointList[0]->type;
						targets[currentType - 2] = lastValue;
						currentValueDelta = targets[currentType - 1] - lastValue;
					}
					value = createSlopeRatioEvents(
							slopeRatio, targets[currentType - 2], currentValueDelta,
							min_[i], max_[i], i);
				} else {
					const auto& point = dynamic_cast<const Transition::Point&>(pointOrSlope);

					if (point.type != currentType) {
						currentType = point.type;
						targets[currentType - 2] = lastValue;
						currentValueDelta = targets[currentType - 1] - lastValue;
					}
					double pointTime;
					Transition::getPointData(point, model_,
									targets[currentType - 2], currentValueDelta, min_[i], max_[i],
									pointTime, value);
					if (!point.isPhantom) {
						insertEvent(i, pointTime, value);
					}
				}
				lastValue = value;
			}
		}
		//else {
		//	insertEvent(i, 0.0, targets[0]);
		//}
	}

	/* Special Event Profiles */
	for (i = 0; i < Parameter::NUM_PARAMETERS; i++) {
		const std::string& specialTransition = rule.getSpecialProfileTransition(i);
		if (!specialTransition.empty()) {
			const Transition* trans = model_.findSpecialTransition(specialTransition); //TODO: check not null
			for (j = 0; j < trans->pointOrSlopeList().size(); ++j) {
				const Transition::PointOrSlope& pointOrSlope = *trans->pointOrSlopeList()[j];
				const auto& point = dynamic_cast<const Transition::Point&>(pointOrSlope);

				/* calculate time of event */
				tempTime = Transition::getPointTime(point, model_);

				/* Calculate value of event */
				value = ((point.value / 100.0) * (max_[i] - min_[i]));
				//maxValue = value;

				/* insert event into event list */
				insertEvent(i + 16, tempTime, value);
			}
		}
	}

	setZeroRef((int) (ruleSymbols[0] * multiplier_) + zeroRef_);
	tempEvent = insertEvent(-1, 0.0, 0.0);
	if (tempEvent) tempEvent->flag = 1;
}

void
EventList::generateEventList()
{
	for (int i = 0; i < 16; i++) {
		const Parameter::Info* tempParameter = &model_.getParameterInfo(i);
		min_[i] = (double) tempParameter->minimum;
		max_[i] = (double) tempParameter->maximum;
	}

	/* Calculate Rhythm including regression */
	for (int i = 0; i < currentFoot_; i++) {
		int rus = feet_[i].end - feet_[i].start + 1;
		/* Apply rhythm model */
		double footTempo;
		if (feet_[i].marked) {
			double tempTempo = 117.7 - (19.36 * (double) rus);
			feet_[i].tempo -= tempTempo / 180.0;
			footTempo = globalTempo_ * feet_[i].tempo;
		} else {
			double tempTempo = 18.5 - (2.08 * (double) rus);
			feet_[i].tempo -= tempTempo / 140.0;
			footTempo = globalTempo_ * feet_[i].tempo;
		}
		for (int j = feet_[i].start; j < feet_[i].end + 1; j++) {
			phoneTempo_[j] *= footTempo;
			if (phoneTempo_[j] < 0.2) {
				phoneTempo_[j] = 0.2;
			} else if (phoneTempo_[j] > 2.0) {
				phoneTempo_[j] = 2.0;
			}
		}
	}

	int basePhoneindex = 0;
	PhoneSequence tempPhoneList;
	while (basePhoneindex < currentPhone_) {
		tempPhoneList.clear();
		for (int i = 0; i < 4; i++) {
			int phoneIndex = basePhoneindex + i;
			if (phoneIndex <= currentPhone_ && phoneData_[phoneIndex].phone) {
				tempPhoneList.push_back(phoneData_[phoneIndex].phone);
			} else {
				break;
			}
		}
		if (tempPhoneList.size() < 2) {
			break;
		}
		int ruleIndex = 0;
		const Rule* tempRule = model_.findFirstMatchingRule(tempPhoneList, ruleIndex);

		ruleData_[currentRule_].number = ruleIndex + 1;

		applyRule(*tempRule, tempPhoneList, &phoneTempo_[basePhoneindex], basePhoneindex);

		basePhoneindex += tempRule->numberOfExpressions() - 1;
	}

	//[dataPtr[numElements-1] setFlag:1];
}

void
EventList::setFullTimeScale()
{
	zeroRef_ = 0;
	zeroIndex_ = 0;
	duration_ = list_.back()->time + 100;
}

void
EventList::applyIntonation()
{
	int tgRandom;
	int firstFoot, endFoot;
	int ruleIndex = 0, phoneIndex;
	int i, j, k;
	double startTime, endTime, pretonicDelta, offsetTime = 0.0;
	double randomSemitone, randomSlope;

	zeroRef_ = 0;
	zeroIndex_ = 0;
	duration_ = list_.back()->time + 100;

	intonationPoints_.clear();

	const Category* vocoidCategory = model_.findCategory("vocoid");

	for (i = 0; i < currentToneGroup_; i++) {
		firstFoot = toneGroups_[i].startFoot;
		endFoot = toneGroups_[i].endFoot;

		startTime = phoneData_[feet_[firstFoot].start].onset;
		endTime = phoneData_[feet_[endFoot].end].onset;

		//printf("Tg: %d First: %d  end: %d  StartTime: %f  endTime: %f\n", i, firstFoot, endFoot, startTime, endTime);

		switch (toneGroups_[i].type) {
		default:
		case TONE_GROUP_TYPE_STATEMENT:
			if (tgUseRandom_) {
				tgRandom = rand() % tgCount_[0];
			} else {
				tgRandom = 0;
			}
			intonParms_ = &tgParameters_[0][tgRandom * 10];
			break;
		case TONE_GROUP_TYPE_EXCLAMATION:
			if (tgUseRandom_) {
				tgRandom = rand() % tgCount_[0];
			} else {
				tgRandom = 0;
			}
			intonParms_ = &tgParameters_[0][tgRandom * 10];
			break;
		case TONE_GROUP_TYPE_QUESTION:
			if (tgUseRandom_) {
				tgRandom = rand() % tgCount_[1];
			} else {
				tgRandom = 0;
			}
			intonParms_ = &tgParameters_[1][tgRandom * 10];
			break;
		case TONE_GROUP_TYPE_CONTINUATION:
			if (tgUseRandom_) {
				tgRandom = rand() % tgCount_[2];
			} else {
				tgRandom = 0;
			}
			intonParms_ = &tgParameters_[2][tgRandom * 10];
			break;
		case TONE_GROUP_TYPE_SEMICOLON:
			if (tgUseRandom_) {
				tgRandom = rand() % tgCount_[3];
			} else {
				tgRandom = 0;
			}
			intonParms_ = &tgParameters_[3][tgRandom * 10];
			break;
		}

		//printf("Intonation Parameters: Type : %d  random: %d\n", toneGroups[i].type, tgRandom);
		//for (j = 0; j<6; j++)
		//	printf("%f ", intonParms[j]);
		//printf("\n");

		pretonicDelta = (intonParms_[1]) / (endTime - startTime);
		//printf("Pretonic Delta = %f time = %f\n", pretonicDelta, (endTime - startTime));

		/* Set up intonation boundary variables */
		for (j = firstFoot; j <= endFoot; j++) {
			phoneIndex = feet_[j].start;
			while (!phoneData_[phoneIndex].phone->isMemberOfCategory(vocoidCategory->code)) {
				phoneIndex++;
				//printf("Checking phone %s for vocoid\n", [phones[phoneIndex].phone symbol]);
				if (phoneIndex > feet_[j].end) {
					phoneIndex = feet_[j].start;
					break;
				}
			}

			if (!feet_[j].marked) {
				for (k = 0; k < currentRule_; k++) {
					if ((phoneIndex >= ruleData_[k].firstPhone) && (phoneIndex <= ruleData_[k].lastPhone)) {
						ruleIndex = k;
						break;
					}
				}

				if (tgUseRandom_) {
					randomSemitone = ((double) rand() / (double) 0x7fffffff) * (double) intonParms_[3] -
								intonParms_[3] / 2.0;
					randomSlope = ((double) rand() / (double) 0x7fffffff) * 0.015 + 0.01;
				} else {
					randomSemitone = 0.0;
					randomSlope = 0.02;
				}

				//printf("phoneIndex = %d onsetTime : %f Delta: %f\n", phoneIndex,
				//	phones[phoneIndex].onset-startTime,
				//	((phones[phoneIndex].onset-startTime)*pretonicDelta) + intonParms[1] + randomSemitone);

				addIntonationPoint((phoneData_[phoneIndex].onset - startTime) * pretonicDelta + intonParms_[1] + randomSemitone,
							offsetTime, randomSlope, ruleIndex);
			} else { /* Tonic */
				if (toneGroups_[i].type == 3) {
					randomSlope = 0.01;
				} else {
					randomSlope = 0.02;
				}

				for (k = 0; k < currentRule_; k++) {
					if ((phoneIndex >= ruleData_[k].firstPhone) && (phoneIndex <= ruleData_[k].lastPhone)) {
						ruleIndex = k;
						break;
					}
				}

				if (tgUseRandom_) {
					randomSemitone = ((double) rand() / (double) 0x7fffffff) * (double) intonParms_[6] -
								intonParms_[6] / 2.0;
					randomSlope += ((double) rand() / (double) 0x7fffffff) * 0.03;
				} else {
					randomSemitone = 0.0;
					randomSlope+= 0.03;
				}
				addIntonationPoint(intonParms_[2] + intonParms_[1] + randomSemitone,
							offsetTime, randomSlope, ruleIndex);

				phoneIndex = feet_[j].end;
				for (k = ruleIndex; k < currentRule_; k++) {
					if ((phoneIndex >= ruleData_[k].firstPhone) && (phoneIndex <= ruleData_[k].lastPhone)) {
						ruleIndex = k;
						break;
					}
				}

				addIntonationPoint(intonParms_[2] + intonParms_[1] + intonParms_[5],
							0.0, 0.0, ruleIndex);
			}
			offsetTime = -40.0;
		}
	}
	addIntonationPoint(intonParms_[2] + intonParms_[1] + intonParms_[5],
				0.0, 0.0, currentRule_ - 1);
}

void
EventList::applyIntonationSmooth()
{
	int j;
	double a, b, c, d;
	double x1, y1, m1, x12, x13;
	double x2, y2, m2, x22, x23;
	double denominator;
	double yTemp;

	setFullTimeScale();
	//tempPoint = [[IntonationPoint alloc] initWithEventList: self];
	//[tempPoint setSemitone: -20.0];
	//[tempPoint setSemitone: -20.0];
	//[tempPoint setRuleIndex: 0];
	//[tempPoint setOffsetTime: 10.0 - [self getBeatAtIndex:(int) 0]];

	//[intonationPoints insertObject: tempPoint at:0];

	for (j = 0; j < intonationPoints_.size() - 1; j++) {
		const IntonationPoint& point1 = *intonationPoints_[j];
		const IntonationPoint& point2 = *intonationPoints_[j + 1];

		x1 = point1.absoluteTime() / 4.0;
		y1 = point1.semitone() + 20.0;
		m1 = point1.slope();

		x2 = point2.absoluteTime() / 4.0;
		y2 = point2.semitone() + 20.0;
		m2 = point2.slope();

		x12 = x1 * x1;
		x13 = x12 * x1;

		x22 = x2 * x2;
		x23 = x22 * x2;

		denominator = x2 - x1;
		denominator = denominator * denominator * denominator;

		d = ( -(y2 * x13) + 3 * y2 * x12 * x2 + m2 * x13 * x2 + m1 * x12 * x22 - m2 * x12 * x22 - 3 * x1 * y1 * x22 - m1 * x1 * x23 + y1 * x23 )
			/ denominator;
		c = ( -(m2 * x13) - 6 * y2 * x1 * x2 - 2 * m1 * x12 * x2 - m2 * x12 * x2 + 6 * x1 * y1 * x2 + m1 * x1 * x22 + 2 * m2 * x1 * x22 + m1 * x23 )
			/ denominator;
		b = ( 3 * y2 * x1 + m1 * x12 + 2 * m2 * x12 - 3 * x1 * y1 + 3 * x2 * y2 + m1 * x1 * x2 - m2 * x1 * x2 - 3 * y1 * x2 - 2 * m1 * x22 - m2 * x22 )
			/ denominator;
		a = ( -2 * y2 - m1 * x1 - m2 * x1 + 2 * y1 + m1 * x2 + m2 * x2) / denominator;

		insertEvent(32, point1.absoluteTime(), point1.semitone());
		//printf("Inserting Point %f\n", [point1 semitone]);
		yTemp = (3.0 * a * x12) + (2.0 * b * x1) + c;
		insertEvent(33, point1.absoluteTime(), yTemp);
		yTemp = (6.0 * a * x1) + (2.0 * b);
		insertEvent(34, point1.absoluteTime(), yTemp);
		yTemp = 6.0 * a;
		insertEvent(35, point1.absoluteTime(), yTemp);
	}
	//[intonationPoints removeObjectAt:0];

	//[self insertEvent:32 atTime: 0.0 withValue: -20.0]; /* A value of -20.0 in bin 32 should produce a
	//							    linear interp to -20.0 */
}

void
EventList::addIntonationPoint(double semitone, double offsetTime, double slope, int ruleIndex)
{
	if (ruleIndex > currentRule_) {
		return;
	}

	IntonationPoint_ptr iPoint(new IntonationPoint(*this));
	iPoint->setRuleIndex(ruleIndex);
	iPoint->setOffsetTime(offsetTime);
	iPoint->setSemitone(semitone);
	iPoint->setSlope(slope);

	double time = iPoint->absoluteTime();
	for (int i = 0; i < intonationPoints_.size(); i++) {
		if (time < intonationPoints_[i]->absoluteTime()) {
			intonationPoints_.insert(intonationPoints_.begin() + i, std::move(iPoint));
			return;
		}
	}

	intonationPoints_.push_back(std::move(iPoint));
}

void
EventList::generateOutput(const char* trmParamFile)
{
	int i, j, k;
	int currentTime, nextTime;
	double currentValues[36];
	double currentDeltas[36];
	double temp;
	float table[16];

	if (list_.empty()) {
		return;
	}
	FILE* fp = fopen(trmParamFile, "a");
	if (fp == NULL) {
		THROW_EXCEPTION(IOException, "Could not open the file " << trmParamFile << '.');
	}

	currentTime = 0;
	for (i = 0; i < 16; i++) {
		j = 1;
		while ((temp = list_[j]->getValue(i)) == INVALID_EVENT_VALUE) {
			j++;
			if (j >= list_.size()) break;
		}
		currentValues[i] = list_[0]->getValue(i);
		if (j < list_.size()) {
			currentDeltas[i] = ((temp - currentValues[i]) / (double) (list_[j]->time)) * 4.0;
		} else {
			currentDeltas[i] = 0.0;
		}
	}
	for (i = 16; i < 36; i++) {
		currentValues[i] = currentDeltas[i] = 0.0;
	}

	if (smoothIntonation_) {
		j = 0;
		while ((temp = list_[j]->getValue(32)) == INVALID_EVENT_VALUE) {
			j++;
			if (j >= list_.size()) break;
		}
		if (j < list_.size()) {
			currentValues[32] = list_[j]->getValue(32);
		} else {
			currentValues[32] = 0.0;
		}
		currentDeltas[32] = 0.0;
	} else {
		j = 1;
		while ((temp = list_[j]->getValue(32)) == INVALID_EVENT_VALUE) {
			j++;
			if (j >= list_.size()) break;
		}
		currentValues[32] = list_[0]->getValue(32);
		if (j < list_.size()) {
			currentDeltas[32] = ((temp - currentValues[32]) / (double) (list_[j]->time)) * 4.0;
		} else {
			currentDeltas[32] = 0.0;
		}
		currentValues[32] = -20.0;
	}

	i = 1;
	currentTime = 0;
	nextTime = list_[1]->time;
	while (i < list_.size()) {

		for (j = 0; j < 16; j++) {
			table[j] = (float) currentValues[j] + (float) currentValues[j + 16];
		}
		if (!microFlag_) table[0] = 0.0;
		if (driftFlag_)  table[0] += driftGenerator_.drift();
		if (macroFlag_)  table[0] += currentValues[32];

		table[0] += pitchMean_;

		if (fp) {
			fprintf(fp,
				"%.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f\n",
				table[0], table[1], table[2], table[3],
				table[4], table[5], table[6], table[7],
				table[8], table[9], table[10], table[11],
				table[12], table[13], table[14], table[15]);
		}

		for (j = 0; j < 32; j++) {
			if (currentDeltas[j]) {
				currentValues[j] += currentDeltas[j];
			}
		}

		if (smoothIntonation_) {
			currentDeltas[34] += currentDeltas[35];
			currentDeltas[33] += currentDeltas[34];
			currentValues[32] += currentDeltas[33];
		} else {
			if (currentDeltas[32]) {
				currentValues[32] += currentDeltas[32];
			}
		}
		currentTime += 4;

		if (currentTime >= nextTime) {
			i++;
			if (i == list_.size()) {
				break;
			}
			nextTime = list_[i]->time;
			for (j = 0; j < 33; j++) { /* 32? 33? */
				if (list_[i - 1]->getValue(j) != INVALID_EVENT_VALUE) {
					k = i;
					while ((temp = list_[k]->getValue(j)) == INVALID_EVENT_VALUE) {
						if (k >= list_.size() - 1) {
							currentDeltas[j] = 0.0;
							break;
						}
						k++;
					}
					if (temp != INVALID_EVENT_VALUE) {
						currentDeltas[j] = (temp - currentValues[j]) /
									(double) (list_[k]->time - currentTime) * 4.0;
					}
				}
			}
			if (smoothIntonation_) {
				if (list_[i - 1]->getValue(33) != INVALID_EVENT_VALUE) {
					currentValues[32] = list_[i - 1]->getValue(32);
					currentDeltas[32] = 0.0;
					currentDeltas[33] = list_[i - 1]->getValue(33);
					currentDeltas[34] = list_[i - 1]->getValue(34);
					currentDeltas[35] = list_[i - 1]->getValue(35);
				}
			}
		}
	}

	fclose(fp);

	if (Log::debugEnabled) {
		printDataStructures();
	}
}

void
EventList::printDataStructures()
{
	printf("Tone Groups %d\n", currentToneGroup_);
	for (int i = 0; i < currentToneGroup_; i++) {
		printf("%d  start: %d  end: %d  type: %d\n", i, toneGroups_[i].startFoot, toneGroups_[i].endFoot,
			toneGroups_[i].type);
	}

	printf("\nFeet %d\n", currentFoot_);
	for (int i = 0; i < currentFoot_; i++) {
		printf("%d  tempo: %f start: %d  end: %d  marked: %d last: %d onset1: %f onset2: %f\n", i, feet_[i].tempo,
			feet_[i].start, feet_[i].end, feet_[i].marked, feet_[i].last, feet_[i].onset1, feet_[i].onset2);
	}

	printf("\nPhones %d\n", currentPhone_);
	for (int i = 0; i < currentPhone_; i++) {
		printf("%d  \"%s\" tempo: %f syllable: %d onset: %f ruleTempo: %f\n",
			 i, phoneData_[i].phone->name().c_str(), phoneTempo_[i], phoneData_[i].syllable, phoneData_[i].onset, phoneData_[i].ruleTempo);
	}

	printf("\nRules %d\n", currentRule_);
	for (int i = 0; i < currentRule_; i++) {
		printf("Number: %d  start: %d  end: %d  duration %f\n", ruleData_[i].number, ruleData_[i].firstPhone,
			ruleData_[i].lastPhone, ruleData_[i].duration);
	}
#if 0
	printf("\nEvents %d\n", list_.size());
	for (int i = 0; i < list_.size(); i++) {
		const Event& event = *list_[i];
		printf("  Event: time=%d flag=%d\n    Values: ", event.time, event.flag);

		for (int j = 0; j < 16; j++) {
			printf("%.3f ", event.getValue(j));
		}
		printf("\n            ");
		for (int j = 16; j < 32; j++) {
			printf("%.3f ", event.getValue(j));
		}
		printf("\n            ");
		for (int j = 32; j < Event::EVENTS_SIZE; j++) {
			printf("%.3f ", event.getValue(j));
		}
		printf("\n");
	}
#endif
}

} /* namespace TRMControlModel */
} /* namespace GS */
