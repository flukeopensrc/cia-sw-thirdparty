/**************************************************************************************/
/* Source Code Module for JPA-SCPI PARSER V1.4.0CPP (C++ version of Parser V1.3.5)		*/
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
/* Implementation of non-class member functions for private use by classes of					*/
/* JPA-SCPI Parser.																																		*/
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
/******* Non-class member functions used by both SCPIParser & SCPIParam classes *******/
/**************************************************************************************/


#ifdef SUPPORT_CHAN_LIST
/**************************************************************************************/
/* Parses an entry in a channel list as a channel range (as defined in SCPI Standard)	*/
/*																																										*/
/* Parameters:																																				*/
/*	[in]	SParam			- Pointer to start of Input Parameter string										*/
/*	[in/out] *pPos		- Pointer to index of starting position within Input Parameter	*/
/*										  Returned as starting position for next entry									*/
/*	[in]	End					- Index of end position in Input Parameter to attempt to parse	*/
/*	[in]	bCheck			- Flag: Non-zero to check channel list attributes								*/
/*	[in]	pSpecAttr		- Pointer to channel list attributes parameter specification		*/
/*																																										*/
/* Return Values:																																			*/
/*	SCPI_ERR_NONE						- OK:			Translation succeeded														*/
/*	SCPI_ERR_PARAM_TYPE			-	Error:	The entry is not a valid channel range					*/
/*  SCPI_ERR_INVALID_VALUE	- Error:  One or more values in the list is invalid, i.e.	*/
/*																		floating point when not allowed, out of range		*/
/*	SCPI_ERR_INVALID_DIMS		- Error:	Invalid number of dimensions in one of more			*/
/*																		of the channel list's entries										*/
/**************************************************************************************/
UCHAR ParseAsChannelRange(char *SParam, SCPI_CHAR_IDX *pPos, SCPI_CHAR_IDX End,
	BOOL bCheck, const struct strSpecAttrChanList *pSpecAttr)
{
	UCHAR Err = SCPI_ERR_NONE;
	SCPIParam sNumParam;
	long lNum;
	BOOL bCompleted = FALSE;
	SCPI_CHAR_IDX NextPos;
	UCHAR DimFirst = 1;
	UCHAR DimLast = 1;
	BOOL bRange = FALSE;

	sNumParam.setType(P_NUM);								/* Set up temporary numeric value parameter for validation purposes */

	while ((*pPos <= End) && (Err != SCPI_ERR_PARAM_TYPE) && !bCompleted)	/* Loop through all characters or until serious error */
	{
		if (sNumParam.PopulateNumericValue(&(SParam[*pPos]), End - (*pPos) + 1, &NextPos) != SCPI_ERR_NONE)
		{																			/* If could not translate as a number 							 */
			Err = SCPI_ERR_PARAM_TYPE;					/* then not a channel list, so set return error code */
			break;															/* and exit the loop 																 */
		}

		/* Validate number according to parameter specifications */
		if (bCheck)														/* If checks required */
		{
			if (!(pSpecAttr->bReal))						/* If real (non-integer) numbers are not allowed */
				if (sNumParam.getNumericValNegExp())	/* If number has a negative exponent i.e. has digits after decimal place */
					Err = SCPI_ERR_INVALID_VALUE;		/* then the value is invalid 																						 */

			if (!(pSpecAttr->bNeg))							/* If negative numbers are not allowed */
				if (sNumParam.getNumericValNeg())	/* If number is negative			*/
					Err = SCPI_ERR_INVALID_VALUE;		/* then the value is invalid	*/

			if (pSpecAttr->bRangeChk)						/* If range checking is required */
			{
				if (sNumParam.SCPI_ToLong(&lNum) == SCPI_ERR_NONE)	/* If value is a valid long number */
				{
					if ((lNum < pSpecAttr->lMin) || (lNum > pSpecAttr->lMax))	/* If value is outside allowed range */
						Err = SCPI_ERR_INVALID_VALUE;														/* then value is invalid 						 */
				}
				else															/* If value is not a valid long integer, e.g. overflows */
					Err = SCPI_ERR_INVALID_VALUE;		/* then value is invalid 																*/
			}
		}

		(*pPos) += NextPos;										/* Move to first character after the number */

		if (*pPos <= End)											/* If there are more characters inside the brackets */
		{
			switch (SParam[*pPos])
			{
			case DIM_SEP:											/* If encountered dimension separator character */
				if (bRange)											/* If in the last part of the entry's range */
					DimLast++;										/* then increment last dimensions counter		*/
				else
					DimFirst++;										/* otherwise increment first dimensions counter */
				break;
			case RANGE_SEP:										/* If encountered a range separator character */
				if (!bRange)										/* If not already decoding a range */
					bRange = TRUE;								/* then set range flag 						 */
				else														/* If already in a range					 														*/
					Err = SCPI_ERR_PARAM_TYPE;		/* then error - cannot have two range separators in one entry */
				break;
			case ENTRY_SEP:										/* If encountered an entry separator character */
				if (bCheck)											/* If checks required */
				{
					if ((DimFirst < pSpecAttr->DimMin) || (DimFirst > pSpecAttr->DimMax))
						Err = SCPI_ERR_INVALID_DIMS;	/* Error if first dimensions are outside allowed limits */
					if (bRange)											/* If entry is a range */
						if ((DimLast < pSpecAttr->DimMin) || (DimLast > pSpecAttr->DimMax) || (DimFirst != DimLast))
							Err = SCPI_ERR_INVALID_DIMS; /* Error if last dimensions outside limits or dimensions do not match */
				}
				bCompleted = TRUE;							/* Reached end of entry so exit this function */
				break;
			default:													/* Any other character */
				Err = SCPI_ERR_PARAM_TYPE;			/* is an error 				 */
			}
			(*pPos)++;													/* Go to next position in parameter */
			if (*pPos > End)										/* If reached end of characters within brackets */
				Err = SCPI_ERR_PARAM_TYPE;				/* then error - must finish on a number 				*/
		}
		else																	/* If there are no more characters after the number */
		{
			if (bCheck)													/* If checks required */
			{
				if ((DimFirst < pSpecAttr->DimMin) || (DimFirst > pSpecAttr->DimMax))
					Err = SCPI_ERR_INVALID_DIMS;		/* Error if first dimensions are outside allowed limits */
				if (bRange)												/* If entry is a range */
					if ((DimLast < pSpecAttr->DimMin) || (DimLast > pSpecAttr->DimMax) || (DimFirst != DimLast))
						Err = SCPI_ERR_INVALID_DIMS;	/* Error if last dimensions outside limits or dimensions do not match */
			}
			bCompleted = TRUE;									/* Reached end of entry so exit this function */
		}
	}
	return Err;
}
#endif


#ifdef SUPPORT_CHAN_LIST_ADV
	/**************************************************************************************/
	/* Parses an entry in a channel list as a module channel (as defined in SCPI Standard)*/
	/*																																										*/
	/* Parameters:																																				*/
	/*	[in]	SParam			- Pointer to start of Input Parameter string										*/
	/*	[in/out] *pPos		- Pointer to index of starting position within Input Parameter	*/
	/*										  Returned as starting position for next entry									*/
	/*	[in]	End					- Index of last position in Input Parameter to attempt to parse	*/
	/*	[in]	bCheck			- Flag: Non-zero to check channel list attributes								*/
	/*	[in]	pSpecAttr		- Pointer to channel list attributes parameter specification		*/
	/*																																										*/
	/* Return Values:																																			*/
	/*	SCPI_ERR_NONE						- OK:			Translation succeeded														*/
	/*	SCPI_ERR_PARAM_TYPE			-	Error:	The entry is not a valid channel range					*/
	/*  SCPI_ERR_INVALID_VALUE	- Error:  One or more values in the list is invalid, i.e.	*/
	/*																		floating point when not allowed, out of range		*/
	/*	SCPI_ERR_INVALID_DIMS		- Error:	Invalid number of dimensions in one of more			*/
	/*																		of the channel list's entries										*/
	/**************************************************************************************/
	UCHAR ParseAsModuleChannel(char *SParam, SCPI_CHAR_IDX *pPos, SCPI_CHAR_IDX End,
		BOOL bCheck, const struct strSpecAttrChanList *pSpecAttr)
	{
		UCHAR Err = SCPI_ERR_NONE;
		SCPIParam sNumParam;
		BOOL bCompleted = FALSE;
		SCPI_CHAR_IDX PosOpenBracket;
		SCPI_CHAR_IDX PosCloseBracket;
		SCPI_CHAR_IDX Pos;

		PosOpenBracket = *pPos;
		while ((PosOpenBracket < End) && !bCompleted)			/* Look for opening bracket in entry */
		{
			if (SParam[PosOpenBracket] == OPEN_BRACKET)			/* Opening bracket found */
				bCompleted = TRUE;
			else
				PosOpenBracket++;
		}
		if (bCompleted)													/* If found opening bracket */
		{
			bCompleted = FALSE;										/* Clear flag */
			PosCloseBracket = PosOpenBracket + 1;
			while ((PosCloseBracket < End) && !bCompleted)	/* Look for closing bracket in entry */
			{
				if (SParam[PosCloseBracket] == CLOSE_BRACKET)	/* Closing bracket found */
					bCompleted = TRUE;
				else
					PosCloseBracket++;
			}
		}
		if (bCompleted)													/* If found opening and closing brackets and no error so far */
		{
			if (PosCloseBracket > PosOpenBracket + 1)		/* If there is at least one char between opening and closing brackets */
			{
				Err = sNumParam.PopulateNumericValue(&(SParam[*pPos]), PosOpenBracket - (*pPos), &Pos);
				if (Err == SCPI_ERR_NONE)
				{																		/* If start of entry translates as a number */
					Pos += *pPos;											/* Make relative to start of Input Parameter string */
					if (Pos != PosOpenBracket)				/* If number does not finish at opening bracket */
						Err = SCPI_ERR_PARAM_TYPE;			/* then the start of entry is not valid as a number */
				}
				if (Err != SCPI_ERR_NONE)						/* If start of entry did not translate as a number */
					Err = ParseAsCharProgData(&(SParam[*pPos]), PosOpenBracket - (*pPos));	/* Parse start of entry as character program data */

				if (Err == SCPI_ERR_NONE)						/* If start of entry transalted as either a number or a string */
				{
					Pos = PosOpenBracket + 1;					/* Start at first character after opening bracket */
					while (Pos < PosCloseBracket)			/* Loop until closing bracket reached */
					{
						Err = ParseAsChannelRange(SParam, &Pos, PosCloseBracket - 1, bCheck, pSpecAttr);	/* Attempt to parse entry as channel range */
						if (Err != SCPI_ERR_NONE)				/* If the channel range is invalid */
							break;												/* then exit this loop */
						while ((iswhitespace(SParam[Pos])) && (Pos < PosCloseBracket))			/* Skip whitespace after channel range */
							Pos++;
					}
				}
			}
			else																	/* No characters between brackets */
				Err = SCPI_ERR_PARAM_TYPE;					/* so return an error 						*/
		}
		else																		/* No opening bracket found */
			Err = SCPI_ERR_PARAM_TYPE;						/* so return an error 			*/

		if (Err == SCPI_ERR_NONE)								/* If no error */
		{
			*pPos = PosCloseBracket + 1;
			bCompleted = FALSE;
			while ((*pPos < End) && !bCompleted)
			{
				if (SParam[*pPos] == ENTRY_SEP)			/* If encountered comma */
					bCompleted = TRUE;								/* then finished        */
				else
					if (!iswhitespace(SParam[*pPos]))	/* if encountered non-whitespace */
					{
						Err = SCPI_ERR_PARAM_TYPE;				/* then that is invalid					 */
						bCompleted = TRUE;
					}
				(*pPos)++;
			}
		}

		return Err;
	}
#endif


#ifdef SUPPORT_CHAN_LIST_ADV
	/**************************************************************************************/
	/* Parses an entry in a channel list as a path name (as defined in SCPI Standard)			*/
	/*																																										*/
	/* Parameters:																																				*/
	/*	[in]	SParam			- Pointer to start of Input Parameter string										*/
	/*	[in/out] *pPos		- Pointer to index of starting position within Input Parameter	*/
	/*										  Returned as starting position for next entry									*/
	/*	[in]	End					- Index of end position in Input Parameter to attempt to parse	*/
	/*																																										*/
	/* Return Values:																																			*/
	/*	SCPI_ERR_NONE						- OK:			Translation succeeded														*/
	/*	SCPI_ERR_PARAM_TYPE			-	Error:	The entry could is not a valid path name				*/
	/**************************************************************************************/
	UCHAR ParseAsPathName(char *SParam, SCPI_CHAR_IDX *pPos, SCPI_CHAR_IDX End)
	{
		UCHAR Err = SCPI_ERR_NONE;
		BOOL bCompleted = FALSE;
		SCPI_CHAR_IDX Pos;

		Pos = *pPos;
		while ((Pos <= End) && !bCompleted)			/* Loop through each character or until comma found */
		{
			if (SParam[Pos] == ENTRY_SEP)					/* If found comma */
			{
				End = Pos - 1;											/* then that is the end of the path name */
				bCompleted = TRUE;
			}
			Pos++;
		}
		Err = ParseAsCharProgData(&(SParam[*pPos]), End - (*pPos) + 1);	/* Check if path name is valid char program data */
		if (Err == SCPI_ERR_NONE)								/* If path name is valid */
			*pPos = End + 2;											/* then next position is after path name */

		return Err;
	}
#endif


#ifdef SUPPORT_CHAN_LIST_ADV
	/**************************************************************************************/
	/* Parses portion of a string as a valid IEEE488.2 (7.7.2) character program data			*/
	/* (whitespace around is allowed)																											*/
	/*																																										*/
	/* Parameters:																																				*/
	/*	[in]	SStart			- Pointer to start of string																		*/
	/*	[in]	Len					- Length of string																							*/
	/*																																										*/
	/* Return Values:																																			*/
	/*	SCPI_ERR_NONE						- OK:			Translation succeeded														*/
	/*	SCPI_ERR_PARAM_TYPE			-	Error:	The entry could is not valid										*/
	/**************************************************************************************/
	UCHAR ParseAsCharProgData(char *SStr, SCPI_CHAR_IDX Len)
	{
		UCHAR Err = SCPI_ERR_NONE;
		UCHAR Pos = 0;

		while (Len > 0)													/* Ignore trailing whitespace */
		{
			if (!iswhitespace(SStr[Len - 1]))
				break;
			Len--;
		}
		while (Pos < Len)												/* Skip over leading whitespace */
		{
			if (!iswhitespace(SStr[Pos]))
				break;
			Pos++;
		}
		if (Pos < Len)
		{
			if (isalpha(SStr[Pos]))							/* If first char is alphabetic */
			{
				Pos++;
				while (Pos < Len)
				{
					if ((!isalnum(SStr[Pos])) && (SStr[Pos] != '_'))		/* If char is not alphanumeric or an underscore */
					{
						Err = SCPI_ERR_PARAM_TYPE;			/* then error has occurred */
						break;													/* and abort loop */
					}
					Pos++;
				}
			}
			else																	/* First char is not alphabetic */
				Err = SCPI_ERR_PARAM_TYPE;					/* so return error code */
		}
		else																		/* Only whitespace */
			Err = SCPI_ERR_PARAM_TYPE;						/* so return error code */

		return Err;
	}
#endif


/**************************************************************************************/
/* Appends a least significant digit to an unsigned long variable's existing value.		*/
/* i.e.:  ulVal(new) = ulVal(old) * Base + Digit																			*/
/*																																										*/
/* Parameters:																																				*/
/*	[in/out] pulVal	- Pointer to unsigned long variable to append digit to;						*/
/*									  if no overflow occurs then this is returned as the new value;		*/
/*										if an overflow does occur then this is returned unchanged.			*/
/*	[in] Digit			- Digit to append (0 to Base-1)																		*/
/*	[in] Base				- Base of number system that Digit is using.											*/
/*																																										*/
/* Return Values:																																			*/
/*	TRUE	- Digit was appended ok																											*/
/*	FALSE - Digit was not be appended as it would have resulted in an overflow of			*/
/*					the unsigned long variable																								*/
/**************************************************************************************/
BOOL AppendToULong(unsigned long *pulVal, char Digit, UCHAR Base)
{
	unsigned long ulNewVal;

	if (*pulVal <= ULONG_MAX / Base)						/* If shifting unsigned long a place to the left will not cause overflow */
	{
		ulNewVal = *pulVal * Base;								/* then shift the value by one place to the left 												 */
		if (ulNewVal <= ULONG_MAX - (unsigned long)Digit)	/* If appending digit will not cause an overflow 	 */
		{
			*pulVal = ulNewVal + (unsigned long)Digit;			/* then append digit to the unsigned long variable */
			return TRUE;																		/* Return no error 																 */
		}
	}
	return FALSE;																/* If overflow would have occurred then return error */
}


#ifdef SUPPORT_NUM_SUFFIX
/**************************************************************************************/
/* Appends a least significant decimal digit to an unsigned integer variable's				*/
/* existing value.																																		*/
/* i.e.:  Val(new) = Val(old) * 10 + Digit																						*/
/*																																										*/
/* Parameters:																																				*/
/*	[in/out] puiVal	- Pointer to unsigned integer variable to append digit to;				*/
/*										if no overflow occurs then this is returned as the new value;		*/
/*										if an overflow does occur then this is returned unchanged.			*/
/*	[in] Digit			- Digit to append (0-9).																					*/
/*																																										*/
/* Return Values:																																			*/
/*	TRUE	- Digit was appended ok																											*/
/*	FALSE - Digit was not be appended as it would have resulted in an overflow of 		*/
/*					the unsigned integer variable																							*/
/**************************************************************************************/
BOOL AppendToUInt(unsigned int *puiVal, char Digit)
{
	unsigned int uiNewVal;

	if (*puiVal <= UINT_MAX / 10)									/* If shifting unsigned integer a place to the left will not cause overflow */
	{
		uiNewVal = *puiVal * 10;									/* then shift value one place to the left													 					*/
		if (uiNewVal <= UINT_MAX - (unsigned int)Digit)	/* If appending digit will not cause an onverflow */
		{
			*puiVal = uiNewVal + (unsigned int)Digit;			/* then append digit to the integer variable			*/
			return TRUE;																	/* Return no error																*/
		}
	}
	return FALSE;																/* If overflow would have occurred then return error */
}
#endif


/**************************************************************************************/
/* Appends a least significant decimal digit to an integer variable's existing value.	*/
/* i.e.:  Val(new) = Val(old) * 10 + Digit																						*/
/*																																										*/
/* Parameters:																																				*/
/*	[in/out] piVal	- Pointer to integer variable to append digit to;									*/
/*										if no overflow occurs then this is returned as the new value;		*/
/*										if an overflow does occur then this is returned unchanged.			*/
/*	[in] Digit			- Digit to append (0-9).																					*/
/*																																										*/
/* Return Values:																																			*/
/*	TRUE	- Digit was appended ok																											*/
/*	FALSE - Digit was not be appended as it would have resulted in an overflow of 		*/
/*					the integer variable																											*/
/**************************************************************************************/
BOOL AppendToInt(int *piVal, char Digit)
{
	int iNewVal;

	if (*piVal <= INT_MAX / 10)										/* If shifting integer a place to the left will not cause overflow */
	{
		iNewVal = *piVal * 10;										/* then shift value one place to the left													 */
		if (iNewVal <= INT_MAX - (int)Digit)			/* If appending digit will not cause an onverflow */
		{
			*piVal = iNewVal + (int)Digit;					/* then append digit to the integer variable			*/
			return TRUE;														/* Return no error																*/
		}
	}
	return FALSE;																/* If overflow would have occurred then return error */
}


/**************************************************************************************/
/* Returns the absolute (positive) value of a signed char variable.										*/
/*																																										*/
/* Parameters:																																				*/
/*	[in] Val	- Signed char value																											*/
/*																																										*/
/* Return Value:																																			*/
/* 	Absolute version of signed value																									*/
/**************************************************************************************/
UCHAR cabs(signed char Val)
{
	if (Val > 0)
		return (UCHAR)Val;
	else
		return (UCHAR)(0 - Val);
}


/**************************************************************************************/
/* Determines if a character is whitespace																						*/
/*																																										*/
/* Parameters:																																				*/
/*	[in]	c - Character																																*/
/*																																										*/
/* Return Values:																																			*/
/*	TRUE 	- Character is whitespace (ASCII codes 1..32)																*/
/*	FALSE - Character is not whitespace																								*/
/**************************************************************************************/
BOOL iswhitespace(char c)
{
	if (c && (c < 33))
		return TRUE;
	else
		return FALSE;
}


/**************************************************************************************/
/* Substitutable Functions																														*/
/* -----------------------																														*/
/* The following functions are also found in the standard C function libraries.				*/
/* If you wish to use those versions of the functions instead, e.g. if your						*/
/* application already makes use those function elsewhere, then:											*/
/*																																										*/
/* 1) Comment out these functions here, and the corresponding function declarations		*/
/*    at the start of this module.																										*/
/* 2) Include the required header files of the standard C libraries in this module.		*/
/**************************************************************************************/


/**************************************************************************************/
/* Determines length of a string																											*/
/*																																										*/
/* Parameters:																																				*/
/*	[in] S	- Null-terminated string.																									*/
/*																																										*/
/* Return Values:																																			*/
/*	Length of string																																	*/
/**************************************************************************************/
SCPI_CHAR_IDX strlen(const char *S)
{
	SCPI_CHAR_IDX Len;

	Len = 0;
	while (S[Len])															/* Loop until reach a null char */
	{
		Len++;
		if (!Len)
			break;																	/* Break if Len overflowed back to 0 (i.e. string too long) */
	}
	return Len;
}


/**************************************************************************************/
/* Converts a character to its lowercase version																			*/
/*																																										*/
/* Parameters:																																				*/
/*	[in] c	- Character																																*/
/*																																										*/
/* Return Value:																																			*/
/*	Lowercase version of character																										*/
/**************************************************************************************/
char tolower(char c)
{
	if (c > 0x40 && c < 0x5B)										/* Characters A..Z				*/
		c += 0x20;																/* are returned as a..z		*/
	return c;
}


/**************************************************************************************/
/* Determines if a character is a lowercase letter																		*/
/*																																										*/
/* Parameters:																																				*/
/*	[in] c	- Character																																*/
/*																																										*/
/* Return Value:																																			*/
/*	TRUE 	- Character is a lowercase letter (a..z)																		*/
/*	FALSE	- Character is not a lowercase letter																				*/
/**************************************************************************************/
BOOL islower(char c)
{
	return ((c > 0x60) && (c < 0x7B)) ? TRUE : FALSE;
}


/**************************************************************************************/
/* Determines if a character is a decimal digit																				*/
/*																																										*/
/* Parameters:																																				*/
/*	[in] c	- Character																																*/
/*																																										*/
/* Return Value:																																			*/
/*	TRUE	- Character is a decimal digit (0..9)																				*/
/*	FALSE	- Character is not a decimal digit																					*/
/**************************************************************************************/
BOOL isdigit(char c)
{
	return ((c > 0x2F) && (c < 0x3A)) ? TRUE : FALSE;
}


#ifdef SUPPORT_CHAN_LIST_ADV
/**************************************************************************************/
/* Determines if a character is alphabetical																					*/
/*																																										*/
/* Parameters:																																				*/
/*	[in] c	- Character																																*/
/*																																										*/
/* Return Value:																																			*/
/*	TRUE	- Character is alphabetical (A..Z, or a..z)																	*/
/*	FALSE	- Character is not alphabetical																							*/
/**************************************************************************************/
BOOL isalpha(char c)
{
	return ((c > 0x40) && (c < 0x5B)) || ((c > 0x60) && (c < 0x7B)) ? TRUE : FALSE;
}
#endif

#ifdef SUPPORT_CHAN_LIST_ADV
/**************************************************************************************/
/* Determines if a character is alphanumeric																					*/
/*																																										*/
/* Parameters:																																				*/
/*	[in] c	- Character																																*/
/*																																										*/
/* Return Value:																																			*/
/*	TRUE	- Character is alphanumeric (A..Z, or a..z, or 0..9)												*/
/*	FALSE	- Character is not alphanumeric																							*/
/**************************************************************************************/
BOOL isalnum(char c)
{
	return (isalpha(c) || isdigit(c)) ? TRUE : FALSE;
}
#endif








