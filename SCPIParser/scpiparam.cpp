/**************************************************************************************/
/* Source Code Module for JPA-SCPI PARSER V1.3.5-CPP (C++ version of Parser V1.3.5)		*/
/*																																										*/
/* (C) JPA Consulting Ltd., 2016	(www.jpacsoft.com)																	*/
/*																																										*/
/* View this file with tab spacings set to 2																					*/
/*																																										*/
/* scpiparam.c																																				*/
/* ===========																																				*/
/*																																										*/
/* Module Description																																	*/
/* ------------------																																	*/
/* Implementation of class SCPIParam, part of JPA-SCPI Parser													*/
/*																																										*/
/* Do not modify this file except where instructed in the code and/or documentation		*/
/*																																										*/
/* Refer to the JPA-SCPI Parser documentation for instructions as to using this code	*/
/* and notes on its design.																														*/
/*																																										*/
/* Module Revision History																														*/
/* -----------------------																														*/
/* V1.0.0:29/07/16:Initial Version 																										*/
/**************************************************************************************/


/* USER: Include any headers required by your compiler here														*/
#include "cmds.h"
#include "scpi.h"
#include "scpiparam.h"
#include "scpiparser.h"
#include "scpipriv.h"


/**************************************************************************************/
/*********************** SCPIParam Class Implementation	*******************************/
/**************************************************************************************/

/**************************************************************************************/
/* Constructor																																				*/
/**************************************************************************************/
SCPIParam::SCPIParam(void)
{
	eType = P_NONE;
};


/**************************************************************************************/
/******************** JPA-SCPI Parser Access Functions ********************************/
/**************************************************************************************/


/**************************************************************************************/
/* Returns the type of the parameter.																									*/
/* If parameter is type Numeric Value, then also returns its sub-type attributes.			*/
/*																																										*/
/* Parameters:																																				*/
/*	[out] pePType -			Pointer to returned type of parameter													*/
/*	[out] pNumSubtype - Pointer to returned parameter's	sub-type attributes,					*/
/*											if parameter is type Numeric Value														*/
/*																																										*/
/* Return Values:																																			*/
/*	SCPI_ERR_NONE			- OK (always)																										*/
/**************************************************************************************/
UCHAR SCPIParam::SCPI_Type(enum enParamType *pePType, UCHAR *pNumSubtype)
{
	*pePType = eType;														/* Set returned type of parameter	*/

	if (eType == P_NUM)													/* If parameter is Numeric Value  */
	{
		*pNumSubtype = 0;
		if (unAttr.sNumericVal.bNeg)							/* If parameter value is negative 													*/
			*pNumSubtype |= SCPI_NUM_ATTR_NEG;			/* then set Negative sub-type attribute											*/
		if (unAttr.sNumericVal.bNegExp)						/* If parameter has a decimal-point (i.e. exponent is -ve)	*/
			*pNumSubtype |= SCPI_NUM_ATTR_REAL;			/* then set Real sub-type attribute													*/
	}

	return SCPI_ERR_NONE;
}


/**************************************************************************************/
/* Returns the units of the parameter if it is type Numeric Value.										*/
/*																																										*/
/* Parameters:																																				*/
/*	[out] peUnits -			Pointer to returned type of units of the parameter						*/
/*																																										*/
/* Return Values:																																			*/
/*	SCPI_ERR_NONE						- OK																											*/
/*	SCPI_ERR_PARAM_TYPE			- Error:	Parameter was not of type Numeric Value					*/
/**************************************************************************************/
UCHAR SCPIParam::SCPI_Units(enum enUnits *peUnits)
{
	if (eType != P_NUM)													/* If parameter is not type Numeric Value */
		return SCPI_ERR_PARAM_TYPE;								/* return error code 												*/

	*peUnits = unAttr.sNumericVal.eUnits;				/* Return units of parameter */

	return SCPI_ERR_NONE;
}


/**************************************************************************************/
/* Converts parameter of type Character Data into its Character Data Item Number.			*/
/*																																										*/
/* Parameters:																																				*/
/*	[out] pItemNum -		Pointer to returned parameter's Character Data Item Number		*/
/*																																										*/
/* Return Values:																																			*/
/*	SCPI_ERR_NONE						- OK																											*/
/*	SCPI_ERR_PARAM_TYPE			- Error:	Parameter was not of type Character Data				*/
/**************************************************************************************/
UCHAR SCPIParam::SCPI_ToCharDataItem(UCHAR *pItemNum)
{
	if (eType != P_CH_DAT)											/* If parameter is not of type Character Data */
		return SCPI_ERR_PARAM_TYPE;								/* return error code													*/

	*pItemNum = (UCHAR)(unAttr.sCharData.ItemNum);	/*	Set returned Item Number */

	return SCPI_ERR_NONE;
}


/**************************************************************************************/
/* Converts parameter of type Boolean into a BOOL																			*/
/*																																										*/
/* Parameters:																																				*/
/*	[out] pbVal -				Pointer to returned BOOL value																*/
/*																																										*/
/* Return Values:																																			*/
/*	SCPI_ERR_NONE						- OK																											*/
/*	SCPI_ERR_PARAM_TYPE			- Error:	Parameter was not of type Boolean								*/
/**************************************************************************************/
UCHAR SCPIParam::SCPI_ToBOOL(BOOL *pbVal)
{
	if (eType != P_BOOL)												/* If parameter is not of type Boolean */
		return SCPI_ERR_PARAM_TYPE;								/* return error code									 */

	if (unAttr.sBoolean.bVal)										/* If parameter value is non-zero			 */
		*pbVal = TRUE;														/* then set returned value to TRUE		 */
	else
		*pbVal = FALSE;														/* else set returned value to FALSE		 */

	return SCPI_ERR_NONE;
}


/**************************************************************************************/
/* Converts parameter of type Numeric Value into an unsigned integer.									*/
/* If parameter's value is negative, then sign is ignored. If parameter's value is		*/
/* real (non-integer) then digits after the decimal point are ignored.								*/
/*																																										*/
/* Parameters:																																				*/
/*	[out] puiVal -			Pointer to returned unsigned int value												*/
/*																																										*/
/* Return Values:																																			*/
/*	SCPI_ERR_NONE						- OK																											*/
/*	SCPI_ERR_PARAM_TYPE			- Error:	Parameter was not of type Numeric Value					*/
/*	SCPI_ERR_PARAM_OVERFLOW - Error:	Value cannot be stored in a variable of type 		*/
/*																		unsigned int																		*/
/**************************************************************************************/
UCHAR SCPIParam::SCPI_ToUnsignedInt(unsigned int *puiVal)
{
	unsigned long ulVal;
	UCHAR Err;

	Err = SCPI_ToUnsignedLong(&ulVal);								/* Convert parameter to unsigned long int */
	if (Err == SCPI_ERR_NONE)										/* If conversion worked ok */
	{
		if (ulVal > UINT_MAX)											/* If value is too big to be stored in an unsigned int */
			Err = SCPI_ERR_PARAM_OVERFLOW;					/* then return error code															 */
		else
			*puiVal = (unsigned int)ulVal;					/* otherwise set returned value */
	}

	return Err;
}


/**************************************************************************************/
/* Converts parameter of type Numeric Value into a signed integer											*/
/* If parameter's value is  real (non-integer) then digits after the decimal point		*/
/* are ignored.																																				*/
/*																																										*/
/* Parameters:																																				*/
/*	[out] piVal -				Pointer to returned signed int value													*/
/*																																										*/
/* Return Values:																																			*/
/*	SCPI_ERR_NONE						- OK																											*/
/*	SCPI_ERR_PARAM_TYPE			- Error:	Parameter was not of type Numeric Value					*/
/*	SCPI_ERR_PARAM_OVERFLOW - Error:	Value cannot be stored in a variable of type int*/
/**************************************************************************************/
UCHAR SCPIParam::SCPI_ToInt(int *piVal)
{
	long lVal;
	UCHAR Err;

	Err = SCPI_ToLong(&lVal);												/* Convert parameter to a signed long int */

	if (Err == SCPI_ERR_NONE)										/* If conversion worked ok */
	{
		if ((lVal > INT_MAX) || (lVal < 0 - INT_MAX))	/* If value is too big or too small to be stored in a signed int */
			Err = SCPI_ERR_PARAM_OVERFLOW;						/* then return error code																				 */
		else
			*piVal = (int)lVal;											/* otherwise set returned value */
	}

	return Err;
}


/**************************************************************************************/
/* Converts parameter of type Numeric Value into an unsigned long											*/
/* If parameter's value is negative, then sign is ignored. If parameter's value is		*/
/* real (non-integer) then digits after the decimal point are ignored.								*/
/*																																										*/
/* Parameters:																																				*/
/*	[out] pulVal -			Pointer to returned unsigned long value												*/
/*																																										*/
/* Return Values:																																			*/
/*	SCPI_ERR_NONE						- OK																											*/
/*	SCPI_ERR_PARAM_TYPE			- Error:	Parameter was not of type Numeric Value					*/
/*	SCPI_ERR_PARAM_OVERFLOW - Error:	Value cannot be stored in a variable of type		*/
/*																		unsigned long																		*/
/**************************************************************************************/
UCHAR SCPIParam::SCPI_ToUnsignedLong(unsigned long *pulVal)
{
	UCHAR Exp;
	UCHAR Err = SCPI_ERR_NONE;

	if (eType != P_NUM)													/* If parameter is not type Numeric Value */
		Err = SCPI_ERR_PARAM_TYPE;								/* then return error code									*/

	else
	{
		*pulVal = unAttr.sNumericVal.ulSigFigs;		/* Get significant-figures component of value */

																											/* Scale according to exponent component of value */
		for (Exp = 0; Exp < cabs(unAttr.sNumericVal.Exp); Exp++)
		{
			if (unAttr.sNumericVal.bNegExp)					/* If exponent is negative	*/
				*pulVal /= 10;												/* divide value by 10				*/
			else																		/* If exponent is positive 	*/
			{
				if (*pulVal <= ULONG_MAX / 10)				/* If another multiplication will not cause an overflow */
					*pulVal *= 10;											/* then multiply value by 10														*/
				else
				{
					Err = SCPI_ERR_PARAM_OVERFLOW;			/* Overflow would occur if multiplied again, so return error */
					break;
				}
			}
		}
	}

	return Err;
}


/**************************************************************************************/
/* Converts parameter of type Numeric Value into a signed long												*/
/* If parameter's value is  real (non-integer) then digits after the decimal point		*/
/* are ignored.																																				*/
/*																																										*/
/* Parameters:																																				*/
/*	[out] plVal -				Pointer to returned signed long value													*/
/*																																										*/
/* Return Values:																																			*/
/*	SCPI_ERR_NONE						- OK																											*/
/*	SCPI_ERR_PARAM_TYPE			- Error:	Parameter was not of type Numeric Value					*/
/*	SCPI_ERR_PARAM_OVERFLOW - Error:	Value cannot be stored in variable of type long */
/**************************************************************************************/
UCHAR SCPIParam::SCPI_ToLong(long *plVal)
{
	unsigned long ulVal;
	UCHAR Err;

	Err = SCPI_ToUnsignedLong(&ulVal);								/* Convert parameter to an unsigned long int */

	if (Err == SCPI_ERR_NONE)										/* If conversion worked ok */
	{
		if (ulVal > LONG_MAX)											/* If value is too big to represent as a signed long */
			Err = SCPI_ERR_PARAM_OVERFLOW;					/* then return error code														 */
		else
		{
			if (unAttr.sNumericVal.bNeg) 						/* If value is negative								*/
				*plVal = 0 - (long)ulVal;							/* then make returned value negative	*/
			else
				*plVal = (long)ulVal;									/* else make returned value positive	*/
		}
	}
	return Err;
}


/**************************************************************************************/
/* Converts parameter of type Numeric Value into a double-precision float							*/
/*																																										*/
/* Parameters:																																				*/
/*	[out] pfdVal -			Pointer to returned double value															*/
/*																																										*/
/* Return Values:																																			*/
/*	SCPI_ERR_NONE						- OK																											*/
/*	SCPI_ERR_PARAM_TYPE			- Error:	Parameter was not of type Numeric Value					*/
/**************************************************************************************/
UCHAR SCPIParam::SCPI_ToDouble(double *pfdVal)
{
	UCHAR Exp;

	if (eType != P_NUM)													/* If parameter is not of type Numeric Value */
		return SCPI_ERR_PARAM_TYPE;								/* return error code													 */

	*pfdVal = (double)(unAttr.sNumericVal.ulSigFigs);	/* Get significant-figures component of value */

																										/* Scale according to exponent component of value */
	for (Exp = 0; Exp < cabs(unAttr.sNumericVal.Exp); Exp++)
	{
		if (unAttr.sNumericVal.bNegExp)						/* If exponent is negative */
			*pfdVal /= 10;													/* divide value by 10			 */
		else																			/* If exponent is positive */
			*pfdVal *= 10;													/* multiply value by 10		 */
	}
	if (unAttr.sNumericVal.bNeg)								/* If parameter value is negative */
		*pfdVal = 0 - (*pfdVal);									/* then negate the returned value	*/

	return SCPI_ERR_NONE;
}


/**************************************************************************************/
/* Converts parameter of type String, Unquoted String or Expression into a pointer		*/
/* to a string and a character count.																									*/
/*																																										*/
/* Parameters:																																				*/
/*	[out] pSString -		Returned pointer to an array of characters containing the			*/
/*											returned string. The array of characters is always within the */
/*											Input String that contained the Input Parameter, and so the 	*/
/*											Input String must still be valid when calling this function.	*/
/*	[out] pLen -				Pointer to number of characters that within returned string.	*/
/*	[out] pDelimiter -	Pointer to returned character that delimited the entered			*/
/*											string parameter, i.e. a double-quote (") or single quote (').*/
/*											This is not applicable if string parameter is unquoted.				*/
/*																																										*/
/* Return Code:																																				*/
/*	SCPI_ERR_NONE						- OK																											*/
/*	SCPI_ERR_PARAM_TYPE			- Error:	Parameter was not type String or Unquoted String*/
/**************************************************************************************/
UCHAR SCPIParam::SCPI_ToString(char **pSString, SCPI_CHAR_IDX *pLen, char *pDelimiter)
{
	if ((eType != P_STR) && (eType != P_UNQ_STR) 						/* If parameter is not type String or Unquoted String */
#ifdef SUPPORT_EXPR
		&& (eType != P_EXPR)																	/* or Expression																			*/
#endif
#ifdef SUPPORT_NUM_LIST
		&& (eType != P_NUM_LIST)															/* or Numeric List																		*/
#endif
#ifdef SUPPORT_CHAN_LIST
		&& (eType != P_CHAN_LIST)															/* or Channel List																		*/
#endif
		)
		return SCPI_ERR_PARAM_TYPE;														/* return error code																	*/

	*pSString = unAttr.sString.pSString;										/* Set returned pointer to start of string */
	*pLen = unAttr.sString.Len;															/* Set returned length of string */
	*pDelimiter = unAttr.sString.Delimiter;									/* Set returned delimiter character */

	return SCPI_ERR_NONE;
}


/*********************************************************************************************************/
/* Converts a parameter of type "Arbituary Block".                                                       */
/*                                                                                                       */
/* Parameters:                                                                                           */ 
/*    [out] ppBlock - Returned pointer to an array of bytes containing the returned data.                */
/*                    The array of bytes is always within the Input Buffer that contained the Input      */
/*                    Parameter, and so the Input Buffer must still be valid when calling this function. */
/*    [out] pLen -    Pointer to number of characters that within returned buffer.                       */
/*                                                                                                       */
/* Return Code:                                                                                          */
/*    SCPI_ERR_NONE       - OK                                                                           */
/*    SCPI_ERR_PARAM_TYPE - Error: Parameter was not type Unquoted String                                */
/*********************************************************************************************************/
UCHAR SCPIParam::SCPI_ToArbitraryBlock(uint8_t** ppBlock, SCPI_CHAR_IDX *pLen)
{

    /* If parameter is not type String or Unquoted String */
    if (eType != P_ARBITRARY_BLOCK)
    {
        return SCPI_ERR_PARAM_TYPE;
    }

    *ppBlock = reinterpret_cast<uint8_t*>(unAttr.sString.pSString);
    *pLen = unAttr.sString.Len;

    return SCPI_ERR_NONE;    
}





#ifdef SUPPORT_NUM_LIST
/**************************************************************************************/
/* Retrieves an entry from Numeric List parameter.																		*/
/*																																										*/
/* Parameters:																																				*/
/*	[in] Index				- Index (0..255) of entry to retrieve, where 0 is first entry		*/
/*	[out] pbRange			- Pointer to returned flag: 1=Entry is a range of values; 			*/
/*											0=Entry is a single value																			*/
/*	[out] psFirst			- Pointer to returned parameter containing entry's value				*/
/*											(or	first value in range if *pbRange==TRUE)										*/
/*	[out] psLast			- Pointer to returned parameter containing entry's last value		*/
/*											in range - only used if *pbRange==TRUE												*/
/*																																										*/
/* Return Values:																																			*/
/*	SCPI_ERR_NONE						- OK:			Entry converted ok.															*/
/*	SCPI_ERR_NO_ENTRY				- Error:	There was no entry to get - the index was				*/
/*																		beyond the end of the entries						 				*/
/*	SCPI_ERR_PARAM_TYPE			- Error:	Parameter was not type Numeric List							*/
/**************************************************************************************/
UCHAR SCPIParam::SCPI_GetNumListEntry(UCHAR Index, BOOL *pbRange, SCPIParam *psFirst, SCPIParam *psLast)
{
	UCHAR Err = SCPI_ERR_NONE;
	SCPI_CHAR_IDX Pos;
	SCPI_CHAR_IDX Len;
	SCPI_CHAR_IDX NextPos;
	UCHAR EntryNum;
	char *pSStr;

	if (eType == P_NUM_LIST)										/* If parameter is type Numeric List */
	{
		Pos = 0;
		EntryNum = 0;
		pSStr = unAttr.sString.pSString;					/* Get pointer to start of Numeric List string */
		Len = unAttr.sString.Len;									/* Get length of Numeric List string */

																							/* Loop through characters of Numeric List string until start of required entry is reached */
		while ((EntryNum < Index) && (Pos < Len))
		{
			if (pSStr[Pos] == ENTRY_SEP)						/* If encountered an entry separating symbol */
				EntryNum++;														/* then increment entry number 							 */
			Pos++;																	/* Go to next character in Numeric List string */
		}

		if (Pos >= Len)														/* If reached end of string without getting to required entry */
			Err = SCPI_ERR_NO_ENTRY;								/* then return error 																					*/
		else
		{
			if (psFirst->PopulateNumericValue(&(pSStr[Pos]), Len - Pos, &NextPos) == SCPI_ERR_NONE)
			{
				psFirst->eType = P_NUM;								/* Set returned parameter as type Numeric Value */
				*pbRange = FALSE;											/* Assume for now that entry is a single value, not a range */
				Pos += NextPos;												/* Go to first character after number */
				if (Pos < Len - 1)
				{
					if (pSStr[Pos] == RANGE_SEP)				/* If character is a range separator symbol */
					{
						*pbRange = TRUE;									/* Entry is a range, not a single value */
						Pos++;														/* Go to character after range separator */

						if (psLast->PopulateNumericValue(&(pSStr[Pos]), Len - Pos, &NextPos) == SCPI_ERR_NONE)
							psLast->eType = P_NUM;
						else															/* If translation of number failed (can't happen with a valid Numeric List) */
							Err = SCPI_ERR_PARAM_TYPE;			/* then return error 							 */
					}
				}
			}
			else																		/* Translation of number failed - can't happen with a valid Numeric List */
				Err = SCPI_ERR_PARAM_TYPE;						/* Return error	*/
		}
	}
	else																				/* Parameter is not type Numeric List */
		Err = SCPI_ERR_PARAM_TYPE;								/* so return error									 	*/

	return Err;
}
#endif


#ifdef SUPPORT_CHAN_LIST_ADV
/**************************************************************************************/
/* Determines the type of entry in Channel List parameter			.												*/
/*																																										*/
/* Parameters:																																				*/
/*	[in] Index				- Index (0..255) of entry to retrieve, where 0 is first entry		*/
/*	[out] peEType			- Pointer to returned type of entry															*/
/* Return Values:																																			*/
/*	SCPI_ERR_NONE						- OK:			Entry converted ok.															*/
/*	SCPI_ERR_NO_ENTRY				- Error:	There was no entry to get - the index was				*/
/*																		beyond the end of the entries										*/
/*	SCPI_ERR_PARAM_TYPE			- Error:	Parameter was not type Channel List							*/
/**************************************************************************************/
UCHAR SCPIParam::SCPI_GetChanListEntryType(UCHAR Index, enum enChanListEntryType *peEType)
{
	UCHAR Err = SCPI_ERR_NONE;
	SCPI_CHAR_IDX	Len;
	SCPI_CHAR_IDX	EntryStart;
	SCPI_CHAR_IDX OrigEntryStart;
	SCPI_CHAR_IDX	EntryEnd;
	char *SStr;
	const struct strSpecAttrChanList *pDummy;

	if (eType == P_CHAN_LIST)										/* If parameter is type Channel List */
	{
		SStr = unAttr.sString.pSString;						/* Get pointer to start of Channel List string */
		Len = unAttr.sString.Len;									/* Get length of Channel List string */
		Err = LocateEntryInChannelList(SStr, Len, Index, &EntryStart, &EntryEnd);

		OrigEntryStart = EntryStart;
		if (Err == SCPI_ERR_NONE)
		{
			if (ParseAsChannelRange(SStr, &EntryStart, EntryEnd, FALSE, pDummy) == SCPI_ERR_NONE)
				*peEType = E_CHANNEL_RANGE;						/* Entry is a valid channel range */
			else
			{
				EntryStart = OrigEntryStart;
				if (ParseAsModuleChannel(SStr, &EntryStart, EntryEnd + 1, FALSE, pDummy) == SCPI_ERR_NONE)
					*peEType = E_MODULE_CHANNEL;				/* Entry is a valid module channel */
				else
				{
					EntryStart = OrigEntryStart;
					if (ParseAsPathName(SStr, &EntryStart, EntryEnd) == SCPI_ERR_NONE)
						*peEType = E_PATH_NAME;						/* Entry is valid path name */
					else
						Err = SCPI_ERR_PARAM_TYPE;				/* No valid type for entry */
				}
			}
		}
	}

	return Err;
}
#endif


#ifdef SUPPORT_CHAN_LIST_ADV
/**************************************************************************************/
/* Retrieves a Channel Range entry of Channel List parameter of type Module Channel		*/
/*																																										*/
/* Parameters:																																				*/
/*	[in] Index				- Index (0..255) of entry to retrieve, where 0 is first entry		*/
/*	[out] psParModuleSpec	- Pointer to returned parameter containing Module Specifier	*/
/*	[out] psParChanRange	- Pointer to returned parameter containing Channel Range		*/
/* Return Values:																																			*/
/*	SCPI_ERR_NONE						- OK:			Entry converted ok.															*/
/*	SCPI_ERR_NO_ENTRY				- Error:	There was no entry to get - the index was				*/
/*																		beyond the end of the entries										*/
/*	SCPI_ERR_PARAM_TYPE			- Error:	Parameter was not type Channel List, or entry		*/
/*																		was not type Module Channel											*/
/**************************************************************************************/
UCHAR SCPIParam::SCPI_GetChanListModuleEntry(UCHAR Index, SCPIParam *psParModuleSpec, SCPIParam *psParChanRange)
{
	UCHAR Err;
	SCPI_CHAR_IDX	Len;
	SCPI_CHAR_IDX	EntryStart;
	SCPI_CHAR_IDX	EntryEnd;
	char *SStr;
	BOOL bCompleted = FALSE;
	SCPI_CHAR_IDX PosOpenBracket;
	SCPI_CHAR_IDX PosCloseBracket;
	SCPI_CHAR_IDX Pos;
	SCPI_CHAR_IDX NextPos;
	const struct strSpecAttrChanList *pDummy;

	if (eType == P_CHAN_LIST)										/* If parameter is type Channel List */
	{
		SStr = unAttr.sString.pSString;						/* Get pointer to start of Channel List string */
		Len = unAttr.sString.Len;									/* Get length of Channel List string */
		Err = LocateEntryInChannelList(SStr, Len, Index, &EntryStart, &EntryEnd);

		if (Err == SCPI_ERR_NONE)
		{
			Pos = EntryStart;                       /* Take copy before altered by function call */
			if (ParseAsModuleChannel(SStr, &EntryStart, EntryEnd + 1, FALSE, pDummy) == SCPI_ERR_NONE)
			{
				PosOpenBracket = Pos;
				while ((PosOpenBracket < EntryEnd) && !bCompleted)	/* Look for opening bracket in entry */
				{
					if (SStr[PosOpenBracket] == OPEN_BRACKET)			/* Opening bracket found */
						bCompleted = TRUE;
					else
						PosOpenBracket++;
				}
				if (bCompleted)												/* If found opening bracket */
				{
					bCompleted = FALSE;									/* Clear flag */
					PosCloseBracket = PosOpenBracket + 1;
					while ((PosCloseBracket <= EntryEnd) && !bCompleted)	/* Look for closing bracket in entry */
					{
						if (SStr[PosCloseBracket] == CLOSE_BRACKET)	/* Closing bracket found */
							bCompleted = TRUE;
						else
							PosCloseBracket++;
					}
				}
				if (bCompleted)												/* If found opening and closing brackets and no error so far */
				{
					Err = psParModuleSpec->PopulateNumericValue(&(SStr[Pos]), PosOpenBracket - Pos, &NextPos);
					if (Err == SCPI_ERR_NONE)
					{																		/* If start of entry translates as a number */
						Pos += NextPos;										/* Make relative to start of Input Parameter string */
						if (Pos != PosOpenBracket)				/* If number does not finishes at opening bracket */
							Err = SCPI_ERR_PARAM_TYPE;			/* then the start of entry is not valid as a number */
					}
					if (Err != SCPI_ERR_NONE)						/* If start of entry did not translate as a number */
					{
						Err = ParseAsCharProgData(&(SStr[Pos]), PosOpenBracket - Pos);	/* Parse start of entry as char program data */
						if (Err == SCPI_ERR_NONE)					/* If start of entry does translate as a string */
						{
							while ((Pos < PosOpenBracket) && (iswhitespace(SStr[Pos])))
								Pos++;												/* Trim leading whitespace */
							Len = PosOpenBracket - Pos;			/* Set length of string */
							while ((Len > 0) && (iswhitespace(SStr[Pos + Len - 1])))
								Len--;												/* Trim trailing whitespace */
							psParModuleSpec->eType = P_UNQ_STR;	/* Mark module specifier as an unquoted string */
							psParModuleSpec->unAttr.sString.pSString = &(SStr[Pos]);		/* Populated string attributes */
							psParModuleSpec->unAttr.sString.Len = Len;
						}
						else															/* If start of entry is not a valid string 				*/
							Err = SCPI_ERR_PARAM_TYPE;			/* then return an error 													*/
					}
				}
				else
					Err = SCPI_ERR_PARAM_TYPE;					/* Matching brackets not found so return an error */

				if (Err == SCPI_ERR_NONE)							/* If success with module specifier */
				{																			/* then populate channel range component */
					psParChanRange->eType = P_CHAN_LIST;	/* Mark it as type channel list */
					psParChanRange->unAttr.sString.pSString = &(SStr[PosOpenBracket + 1]);	/* Start is char after opening bracket */
					psParChanRange->unAttr.sString.Len = PosCloseBracket - PosOpenBracket - 1;	/* Set length to not include closing bracket */
				}
			}
			else
				Err = SCPI_ERR_PARAM_TYPE;						/* Not a valid Matching brackets not found so return an error */
		}
		else
			Err = SCPI_ERR_PARAM_TYPE;							/* Entry is not a valid module channel */
	}
	else
		Err = SCPI_ERR_PARAM_TYPE;								/* Parameter is not a channel list */

	return Err;
}
#endif


#ifdef SUPPORT_CHAN_LIST_ADV
/**************************************************************************************/
/* Retrieves the contents of Channel List parameter	of type Path Name									*/
/*																																										*/
/* Parameters:																																				*/
/*	[in] Index				- Index (0..255) of entry to retrieve, where 0 is first entry		*/
/*	[out] pSPath			- Pointer to returned start of path name string									*/
/*	[out] pPathLen		- Pointer to returned length of path name string								*/
/* Return Values:																																			*/
/*	SCPI_ERR_NONE						- OK:			Entry converted ok.															*/
/*	SCPI_ERR_NO_ENTRY				- Error:	There was no entry to get - the index was				*/
/*																		beyond the end of the entries										*/
/*	SCPI_ERR_PARAM_TYPE			- Error:	Parameter was not type Channel List, or entry		*/
/*																		was not type Path Name													*/
/**************************************************************************************/
UCHAR SCPIParam::SCPI_GetChanListPathEntry(UCHAR Index, char **pSPath, SCPI_CHAR_IDX *pPathLen)
{
	UCHAR Err;
	SCPI_CHAR_IDX	Len;
	SCPI_CHAR_IDX	EntryStart;
	SCPI_CHAR_IDX CopyEntryStart;
	SCPI_CHAR_IDX	EntryEnd;
	char *SStr;

	if (eType == P_CHAN_LIST)										/* If parameter is type Channel List */
	{
		SStr = unAttr.sString.pSString;						/* Get pointer to start of Channel List string */
		Len = unAttr.sString.Len;									/* Get length of Channel List string */
		Err = LocateEntryInChannelList(SStr, Len, Index, &EntryStart, &EntryEnd);

		if (Err == SCPI_ERR_NONE)
		{
			CopyEntryStart = EntryStart;
			if (ParseAsPathName(SStr, &CopyEntryStart, EntryEnd) == SCPI_ERR_NONE)
			{																		/* Entry is valid path name */
				while ((EntryStart < EntryEnd) && (iswhitespace(SStr[EntryStart])))
					EntryStart++;										/* Trim leading whitespace */
				while ((EntryEnd > EntryStart) && (iswhitespace(SStr[EntryEnd])))
					EntryEnd--;											/* Trim trailing whitespace */
				*pSPath = &(SStr[EntryStart]);					/* Populate return param with start of path name string */
				*pPathLen = EntryEnd - EntryStart + 1;	/* Populate return param with length of path name string */
			}
			else
				Err = SCPI_ERR_PARAM_TYPE;				/* Entry is not a valid path name */
		}
	}
	else
		Err = SCPI_ERR_PARAM_TYPE;						/* Parameter is not a channel list */

	return Err;
}
#endif


#ifdef SUPPORT_CHAN_LIST
/**************************************************************************************/
/* Retrieves a channel range entry from Channel List parameter.												*/
/*																																										*/
/* Parameters:																																				*/
/*	[in] Index				- Index (0..255) of entry to retrieve, where 0 is first entry		*/
/*	[in/out] pDims		- Inwards: Pointer to maximum dimensions possible in an entry;	*/
/*											returned as the number of dimensions in the entry							*/
/*	[out] pbRange			- Pointer to returned flag: 1=Entry is a range of values; 			*/
/*											0=Entry is a single value																			*/
/*	[out] sFirst[]		- Array [0..Dims-1] of returned parameters containing entry's		*/
/*											value (or	first value in range if *pbRange==TRUE)							*/
/*	[out] sLast[]			- Array [0..Dims-1] of returned parameters containing entry's		*/
/*											last value in range - only used if *pbRange==TRUE							*/
/*																																										*/
/* Return Values:																																			*/
/*	SCPI_ERR_NONE						- OK:			Entry converted ok.															*/
/*	SCPI_ERR_NO_ENTRY				- Error:	There was no entry to get - the index was				*/
/*																		beyond the end of the entries										*/
/*	SCPI_ERR_TOO_MANY_DIMS	- Error:	Too many dimensions in entry to be returned 		*/
/*																		in parameters.																	*/
/*	SCPI_ERR_PARAM_TYPE			- Error:	Parameter was not type Channel List							*/
/**************************************************************************************/
UCHAR SCPIParam::SCPI_GetChanListEntry(UCHAR Index, UCHAR *pDims, BOOL *pbRange, SCPIParam sFirst[], SCPIParam sLast[])
{
	UCHAR Err = SCPI_ERR_NONE;
	SCPI_CHAR_IDX End;
	SCPI_CHAR_IDX Pos;
	SCPI_CHAR_IDX Len;
	SCPI_CHAR_IDX NextPos;
	char *pSStr;
	UCHAR Dim;
	SCPIParam *psCurParam;

	if (eType == P_CHAN_LIST)										/* If parameter is type Channel List */
	{
		Pos = 0;
		pSStr = unAttr.sString.pSString;					/* Get pointer to start of Channel List string */
		Len = unAttr.sString.Len;									/* Get length of Channel List string */

		Err = LocateEntryInChannelList(pSStr, Len, Index, &Pos, &End);		/* :Locate entry within parameter */
		if (Err == SCPI_ERR_NONE)
		{
			Dim = 0;
			*pbRange = FALSE;

			while ((Pos <= End) && (Err == SCPI_ERR_NONE))		/* Loop through all characters of entry or until error */
			{
				if (Dim < *pDims)											/* If current dimension is within allowed dimension count */
				{
					if (!*pbRange)											/* Set pointer to returned parameter to be populated */
						psCurParam = &(sFirst[Dim]);
					else
						psCurParam = &(sLast[Dim]);
				}
				else																	/* If reached too many dimensions */
				{
					Err = SCPI_ERR_TOO_MANY_DIMS;				/* then set error code						*/
					break;															/* and exit the loop							*/
				}
				if (psCurParam->PopulateNumericValue(&(pSStr[Pos]), End - Pos + 1, &NextPos) == SCPI_ERR_NONE)
				{
					psCurParam->eType = P_NUM;					/* And set returned parameter type as numeric value */

					Pos += NextPos;											/* Go to first character after number */
					if (Pos < End)
					{
						switch (pSStr[Pos])
						{
						case DIM_SEP:										/* If character is a dimension separator */
							Dim++;												/* then increment dimension counter 		 */
							Pos++;												/* Go forward to next character */
							break;
						case RANGE_SEP:									/* If character is a range separator 	*/
							*pbRange = TRUE;							/* then now into second part of range */
							Dim = 0;											/* Reset dimension counter */
							Pos++;												/* Go forward to next character */
							break;
						default:												/* Any other character - this can't happen with a valid Channel List */
							Err = SCPI_ERR_PARAM_TYPE;		/* return error code in this case */
						}
					}
				}
				else																	/* Could not translate number - this can't happen with a valid Channel List */
					Err = SCPI_ERR_PARAM_TYPE;					/* return error code in this case */
			}
			*pDims = Dim + 1;												/* Return the number of dimensions in the entry */
		}
	}
	else																					/* Parameter is not type Numeric List */
		Err = SCPI_ERR_PARAM_TYPE;									/* so return error									 	*/

	return Err;
}
#endif


/**************************************************************************************/
/*********************** Other Public Class Member Functions **************************/
/**************************************************************************************/

#ifdef SUPPORT_CHAN_LIST
/**************************************************************************************/
/* Locates single entry within a channel list																					*/
/* Parameters:																																				*/
/*	[in]	SStr				- Pointer to string containing channel list											*/
/*	[in]	Len					- Length of string																							*/
/*	[in]	Index				- Index (where 0 is first) of entry to locate										*/
/*	[out]	pStart			- Returned as pointer to position in string that is entry start	*/
/*	[out]	pEnd				- Returned as pointer to position in string that is entry end		*/
/*																																										*/
/* Return Values:																																			*/
/*	SCPI_ERR_NONE						- OK:			Translation succeeded														*/
/*	SCPI_ERR_NO_ENTRY				- Error:	No entry with index given												*/
/*	SCPI_ERR_PARAM_TYPE			-	Error:	No valid entry located													*/
/**************************************************************************************/
UCHAR SCPIParam::LocateEntryInChannelList(char *SStr, SCPI_CHAR_IDX Len, UCHAR Index, SCPI_CHAR_IDX *pStart, SCPI_CHAR_IDX *pEnd)
{
	UCHAR Err;
	BOOL bCompleted = FALSE;
	BOOL bWithinBrackets = FALSE;
	SCPI_CHAR_IDX Pos = 0;

	Pos = 0;
	*pStart = Pos;
	while (!bCompleted && (Pos < Len))					/* Loop until found entry or reached end of string */
	{
		switch (SStr[Pos])
		{
		case ENTRY_SEP:													/* Comma encountered */
			if (!bWithinBrackets)									/* If not inside brackets */
			{
				if (!Index)													/* If dealing with the entry of interest */
				{
					*pEnd = Pos - 1;									/* Populate return param with end of entry */
					bCompleted = TRUE;
					Err = SCPI_ERR_NONE;
				}
				else																/* If not dealing with the entry of interest */
				{
					Index--;													/* then decrement index counter */
					*pStart = Pos + 1;								/* and set start of entry to position after comma */
				}
			}
			break;
		case OPEN_BRACKET:											/* Opening bracket encountered */
			bWithinBrackets = TRUE;								/* so set flag 								 */
			break;
		case CLOSE_BRACKET:											/* Closing bracket encountered */
			bWithinBrackets = FALSE;							/* so clear flag							 */
			break;
		}
		Pos++;
	}
	if (!bCompleted)														/* If reached end of string	*/
	{
		if (!Index)																/* If at the correct index */
		{
			if (!bWithinBrackets)										/* If not inside brackets */
			{
				*pEnd = Pos - 1;											/* then found the entry */
				Err = SCPI_ERR_NONE;
			}
			else
				Err = SCPI_ERR_PARAM_TYPE;						/* Within brackets at end so invalid entry */
		}
		else
			Err = SCPI_ERR_NO_ENTRY;								/* No such entry with the given index */
	}
	return Err;
}
#endif


/**************************************************************************************/
/* Translates part of a string into a Number and populates the Numeric Value property.*/
/* Translation stops when a non-numeric character is encountered or the length of			*/
/* string to be translated has been reached.																					*/
/*																																										*/
/* Parameters:																																				*/
/*	[in]			SNum			- Pointer to start of string containing the number.						*/
/*	[in]			Len				- Length of string to translate up to.												*/
/*	[out]			psNum			- Pointer to returned numeric value structure									*/
/*												(contents are undefined if an error code is returned)				*/
/*	[out]			pNextPos 	- Pointer to returned index position within string of first		*/
/*											 	non-whitespace character after the number.									*/
/*												(contents are undefined if an error code is returned)				*/
/*																																										*/
/* Return Values:																																			*/
/*	SCPI_ERR_NONE						- OK:			Translation succeeded														*/
/*	SCPI_ERR_PARAM_TYPE			-	Error:	The Input Parameter was the wrong type for the	*/
/*																		Parameter Spec																	*/
/*	SCPI_ERR_PARAM_OVERFLOW	- Error:	The Input Parameter value	overflowed (exponent	*/
/*																		was greater	than +/-43)													*/
/**************************************************************************************/
UCHAR SCPIParam::PopulateNumericValue(char *SNum, SCPI_CHAR_IDX Len, SCPI_CHAR_IDX *pNextPos)
{
	UCHAR Err = SCPI_ERR_NONE;
	UCHAR State = 0;
	char Ch;
	SCPI_CHAR_IDX Pos = 0;
	BOOL bFoundDigit = FALSE;
	BOOL bDigit;
	char Digit;
	BOOL bIsWhitespace;
	unsigned long ulSigFigs = 0;
	BOOL bNeg = FALSE;
	BOOL bNegExp = FALSE;
	int iExp = 0;
	int iDecPlaces = 0;
	BOOL bSigFigsLost = FALSE;									/* Flag - If TRUE then significant figures have been lost in translation */

																							/* Parse each character in string, until end of string, an error occurs, or finished parsing */
	while ((Pos < Len) && (Err == SCPI_ERR_NONE) && (State != 8))
	{
		Ch = tolower(SNum[Pos]);									/* Read character from Input Parameter string */
		bDigit = !!isdigit(Ch);									/* Set bDigit to TRUE if character is a decimal digit */
		if (bDigit)																/* If character is a digit */
			Digit = Ch - '0';												/* then get value of digit (0..9) */
		bIsWhitespace = iswhitespace(Ch);				/* Set bIsWhitespace to TRUE if character is whitespace */

																						/* FSM (Finite State Machine) for parsing Numeric Values				*/
																						/* Refer to Design Notes document for a description of this FSM	*/
		switch (State)
		{
		case 0:
			if (bDigit)
			{
				ulSigFigs = (unsigned long)Digit;		/* Set first digit of significant figures */
				bFoundDigit = TRUE;
				State = 2;
			}
			else
				switch (Ch)
				{
				case '.': State = 3; break;				/* Decimal point */
				case '+': State = 1; break;				/* Positive decimal number */
				case '-': bNeg = TRUE; State = 1; /* Negative decimal number */
					break;
				case '#': State = 10; break;			/* Symbol that precedes non-decimal number */
				default:
					if (!bIsWhitespace)	 						/* Whitespace is allowed - just repeat this state */
						Err = SCPI_ERR_PARAM_TYPE;		/* But any other character is invalid */
				}
			break;

		case 1:
			if (bDigit)														/* If character is a digit */
			{
				AppendToULong(&ulSigFigs, Digit, BASE_DEC);	/* Append digit to significant figures
																										Note: Cannot lose significant figures at this stage */
				bFoundDigit = TRUE;
				State = 2;
			}
			else
				if (Ch == '.')											/* Decimal point */
					State = 3;
				else																/* Invalid character encountered for this state */
					Err = SCPI_ERR_PARAM_TYPE;				/* so abort translation												  */
			break;

		case 2:
			if (bDigit)														/* If character is a digit */
			{
				if (!AppendToULong(&ulSigFigs, Digit, BASE_DEC)) /* Append digit to significant figures 	*/
				{																		/* If it was not possible to append the digit 				*/
					bSigFigsLost = TRUE;							/* then flag that significant figures have been lost 	*/
					iDecPlaces--;											/* and decrement decimal places (so it goes negative)	*/
				}
			}
			else																	/* If character is not a digit */
				if (Ch == '.')											/* Decimal point */
					State = 3;
				else
				{
					if (Ch == 'e')										/* Exponent character encountered */
						State = 5;
					else
						if (bIsWhitespace)							/* Whitespace encountered */
							State = 9;
						else														/* Other character encountered (the start of the Units) */
							State = 8;
				}
			break;

		case 3:
			if (bDigit)														/* If character is a digit */
			{
				if (!bSigFigsLost)									/* Only append decimal place digits if no significant figures lost yet */
				{
					if (AppendToULong(&ulSigFigs, Digit, BASE_DEC))	/* Append digit to significant figures	 	*/
						iDecPlaces++;																		/* and increment count of decimal places 	*/
					else															/* If not possible to append digit to significant figures */
						bSigFigsLost = TRUE;						/* then flag that significant figures have been lost			*/
				}
				bFoundDigit = TRUE;
				State = 4;
			}
			else																	/* If character is not a digit */
			{
				if (Ch == 'e')											/* Exponent character encountered */
					State = 5;
				else																/* Whitespace or other char encountered */
					State = 8;												/* so finished reading number						*/
			}
			break;

		case 4:
			if (bDigit)														/* If character is a digit */
			{
				if (!bSigFigsLost)									/* Only append decimal place digits if no significant figures lost yet */
				{
					if (AppendToULong(&ulSigFigs, Digit, BASE_DEC))	/* Append digit to significant figures		*/
						iDecPlaces++;																		/* and increment count of decimal places	*/
					else															/* If not possible to append digit to significant figures	*/
						bSigFigsLost = TRUE;						/* then flag that significant figures have been lost			*/
				}
			}
			else																	/* If character is not a digit */
				if (Ch == 'e')											/* Exponent character encountered */
					State = 5;
				else
					if (bIsWhitespace)								/* Whitespace or other char encountered */
						State = 9;
					else															/* Any other char encountered */
						State = 8;											/* so finished reading number */
			break;

		case 5:
			if (bDigit)														/* If character is a digit */
			{
				iExp = (int)Digit;									/* Set first digit of exponent */
				State = 7;
			}
			else																	/* If character is not a digit */
				if (Ch == '+')											/* Plus-sign encountered */
					State = 6;
				else
					if (Ch == '-')										/* Minus-sign encountered */
					{
						bNegExp = TRUE;									/* Exponent is negative */
						State = 6;
					}
					else
						if (!bIsWhitespace)							/* Whitespace encountered - ignore it (stay in present state) */
							Err = SCPI_ERR_PARAM_TYPE;		/* Any other char is invalid in this state */
			break;

		case 6:
			if (bDigit)														/* If character is a digit */
			{
				iExp = (int)Digit;									/* Set first digit of exponent */
				State = 7;
			}
			else																	/* Invalid character encountered for this state */
				Err = SCPI_ERR_PARAM_TYPE;
			break;

		case 7:
			if (bDigit)														/* If character is a digit */
			{
				if (!AppendToInt(&iExp, Digit))		/* Append digit to end of exponent */
					Err = SCPI_ERR_PARAM_OVERFLOW;		/* If not possible to append digit, return an overflow error */
			}
			else																	/* Whitespace of other char encountered */
				State = 8;													/* so finished reading number 					*/
			break;

		case 9:
			if (Ch == 'e')												/* Exponent character encountered */
				State = 5;
			else
				if (!bIsWhitespace)									/* If any other char apart from whitespace */
					State = 8;												/* then finished reading number						 */
			break;

		case 10:
			switch (Ch)
			{
			case 'b': State = 11; break;				/* Symbol for Binary number 			*/
			case 'q': State = 12; break;				/* Symbol for Octal number				*/
			case 'h': State = 13; break;				/* Symbol for Hexadecimal number	*/
			default: Err = SCPI_ERR_PARAM_TYPE; /* Any other char is invalid for this state */
			}
			break;

		case 11:
			if (bDigit && (Digit < BASE_BIN))			/* If character is a valid binary digit */
			{
				bFoundDigit = TRUE;
				if (!AppendToULong(&ulSigFigs, Digit, BASE_BIN)) /* Append binary digit to significant figures */
					Err = SCPI_ERR_PARAM_OVERFLOW;		/* If cannot append digit then return overflow error */
			}
			else																	/* If character is not a valid binary digit */
				State = 8;													/* stop reading number											*/
			break;

		case 12:
			if (bDigit && (Digit < BASE_OCT))			/* If character is a valid octal digit */
			{
				bFoundDigit = TRUE;
				if (!AppendToULong(&ulSigFigs, Digit, BASE_OCT)) /* Append octal digit to significant figures */
					Err = SCPI_ERR_PARAM_OVERFLOW;		/* If cannot append digit then return overflow error code */
			}
			else																	/* If character is not a valid octal digit */
				State = 8;													/* stop reading number 										 */
			break;

		case 13:
			if (Ch >= 'a' && Ch <= 'f')						/* If character in range a-f */
			{
				bDigit = TRUE;											/* then this is a valid hexadecimal digit */
				Digit = Ch - 'a' + 10;							/* and set Digit to value (10-15) */
			}
			if (bDigit)														/* If valid hexadecimal digit */
			{
				bFoundDigit = TRUE;
				if (!AppendToULong(&ulSigFigs, Digit, BASE_HEX)) /* Append hexadecimal digit to significant figures */
					Err = SCPI_ERR_PARAM_OVERFLOW;		/* If cannot append digit then return overflow error */
			}
			else																	/* If character is not a valid hexadecimal digit */
				State = 8;													/* stop reading number													 */
			break;
		}

		if (State != 8)														/* If not at End state 							*/
			Pos++;																	/* then move position to next char  */
	}

	while (Pos < Len)
	{
		if (iswhitespace(SNum[Pos]))							/* Move position to first non-whitespace char after number */
			Pos++;
		else
			break;
	}

	if (!bFoundDigit)														/* If Input Parameter string contains no digits 											*/
		Err = SCPI_ERR_PARAM_TYPE;								/* then parameter is not a number (e.g. "+." or "." are not numbers)	*/

																							/* Determine if the exit state from FSM was a valid one to end on */
	switch (State)
	{
	case 2: case	3: case	 4: case	7: case  8:
	case 9: case 11: case 12: case 13:				/* It is valid to exit the FSM in any of these states */
		break;
	default:																	/* It is invalid to exit the FSM in any other state 	*/
		Err = SCPI_ERR_PARAM_TYPE;							/* so return error code																*/
	}

	if (Err == SCPI_ERR_NONE)
	{
		if (bNegExp)															/* If exponent is flagged as negative */
			iExp = 0 - iExp;												/* then make the exponent negative		*/

		iExp -= iDecPlaces;												/* Adjust exponent by the number of decimal places */

																							/* Get rid of all non-significant figures in integer component */
		while ((ulSigFigs % 10 == 0) && ulSigFigs) /* Loop while least significant figure digit is zero */
		{
			ulSigFigs = ulSigFigs / 10;							/* Remove last zero 											*/
			iExp++;																	/* and increment exponent to balance this	*/
		}

		if (iExp > MAX_EXPONENT)									/* If exponent is too big															*/
			Err = SCPI_ERR_PARAM_OVERFLOW;					/* then exit function, returning overflow error code	*/

		if (iExp < MIN_EXPONENT)									/* If exponent is too small (i.e. too many decimal places */
		{																					/* before the first significant digit)										*/
			ulSigFigs = 0;													/* then the number is effectively zero										*/
			iExp = 0;
		}

		if (iExp < 0)															/* If exponent is negative							*/
		{
			iExp = 0 - iExp;												/* Make it positive											*/
			bNegExp = TRUE;													/* and set flag "exponent is negative"	*/
		}
		else																			/* If exponent is positive						*/
			bNegExp = FALSE;												/* clear flag "exponent is negative"	*/
	}

	if (Err == SCPI_ERR_NONE)
	{
		/* Populate numeric value properties */
		unAttr.sNumericVal.ulSigFigs = ulSigFigs;
		unAttr.sNumericVal.bNeg = bNeg;
		unAttr.sNumericVal.Exp = cabs((signed char)iExp);
		unAttr.sNumericVal.bNegExp = bNegExp;
		eType = P_NUM;

		*pNextPos = Pos;													/* Return index of next char after number	*/
	}

	return Err;
}


/**************************************************************************************/
/********************* Property Access (Set/Get) Functions ****************************/
/**************************************************************************************/

/**************************************************************************************/
/* Type property																																			*/
/**************************************************************************************/
enum enParamType SCPIParam::getType(void)
{
	return eType;
}
void SCPIParam::setType(enum enParamType eNewType)
{
	eType = eNewType;
}


/**************************************************************************************/
/*  Number of Character Data Item within Sequence property														*/
/**************************************************************************************/
void SCPIParam::setCharDataItemNum(unsigned char ItemNum)
{
	unAttr.sCharData.ItemNum = ItemNum;
}


/**************************************************************************************/
/*  Character Sequence property																												*/
/**************************************************************************************/
void SCPIParam::setCharDataSeq(const char *SSeq)
{
	unAttr.sCharData.SSeq = SSeq;
};


/**************************************************************************************/
/*  Boolean Value property																														*/
/**************************************************************************************/
void SCPIParam::setBooleanVal(BOOL bVal)
{
	unAttr.sBoolean.bVal = bVal;
}


/**************************************************************************************/
/*  Numeric Value property																														*/
/**************************************************************************************/
struct strAttrNumericVal SCPIParam::getNumericVal(void)
{
	return unAttr.sNumericVal;
}

/**************************************************************************************/
/*  Numeric Value Negative property (TRUE=Value is negative)													*/
/**************************************************************************************/
void SCPIParam::setNumericValNeg(BOOL bNeg)
{
	unAttr.sNumericVal.bNeg = bNeg;
}

BOOL SCPIParam::getNumericValNeg(void)
{
	return unAttr.sNumericVal.bNeg;
}


/**************************************************************************************/
/*  Numeric Value Significant Figures property																				*/
/**************************************************************************************/
void SCPIParam::setNumericValSigFigs(unsigned long ulSigFigs)
{
	unAttr.sNumericVal.ulSigFigs = ulSigFigs;
}

unsigned long SCPIParam::getNumericValSigFigs(void)
{
	return unAttr.sNumericVal.ulSigFigs;
}


/**************************************************************************************/
/*  Numeric Value Exponent property																										*/
/**************************************************************************************/
void SCPIParam::setNumericValExp(unsigned char Exp)
{
	unAttr.sNumericVal.Exp = Exp;
}

unsigned char SCPIParam::getNumericValExp(void)
{
	return unAttr.sNumericVal.Exp;
}

/**************************************************************************************/
/*  Numeric Value Negative Exponent property (TRUE=Exponent is negative)							*/
/**************************************************************************************/
void SCPIParam::setNumericValNegExp(BOOL bNegExp)
{
	unAttr.sNumericVal.bNegExp = bNegExp;
}

BOOL SCPIParam::getNumericValNegExp(void)
{
	return unAttr.sNumericVal.bNegExp;
}

/**************************************************************************************/
/*  Numeric Value Type of Units property																							*/
/**************************************************************************************/
void SCPIParam::setNumericValUnits(enum enUnits	eUnits)
{
	unAttr.sNumericVal.eUnits = eUnits;
}

enum enUnits SCPIParam::getNumericValUnits(void)
{
	return unAttr.sNumericVal.eUnits;
}



/**************************************************************************************/
/*  Pointer to Start of String property																								*/
/**************************************************************************************/
void SCPIParam::setString(char *pSString)
{
	unAttr.sString.pSString = pSString;
}


/**************************************************************************************/
/*  Length of String property																													*/
/**************************************************************************************/
void SCPIParam::setStringLen(SCPI_CHAR_IDX Len)
{
	unAttr.sString.Len = Len;
}


/**************************************************************************************/
/* String Delimiter property																													*/
/**************************************************************************************/
void SCPIParam::setStringDelimiter(char Delimiter)
{
	unAttr.sString.Delimiter = Delimiter;
}
