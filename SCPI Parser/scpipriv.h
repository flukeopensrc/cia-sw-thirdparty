/**************************************************************************************/
/* Source Code Module for JPA-SCPI PARSER V1.4.0CPP (C++ version of Parser V1.3.5)		*/
/*																																										*/
/* (C) JPA Consulting Ltd., 2016	(www.jpacsoft.com)																	*/
/*																																										*/
/* View this file with tab spacings set to 2																					*/
/*																																										*/
/* scpipriv.h																																					*/
/* ==========																																					*/
/*																																										*/
/* Module Description																																	*/
/* ------------------																																	*/
/* Declarations of constants and non-class member functions for private use by				*/
/* classes of JPA-SCPI Parser.																												*/
/*																																										*/
/* Do not modify this file except where instructed in the code and/or documentation		*/
/*																																										*/
/* Refer to the JPA-SCPI Parser documentation for instructions as to using this code	*/
/* and notes on its design.																														*/
/*																																										*/
/* Module Revision History																														*/
/* -----------------------																														*/
/* V1.0.0:31/07/16:First version																											*/
/**************************************************************************************/


/* Only include this header file once */
#ifndef SCPIPRIV_H
#define SCPIPRIV_H


/**************************************************************************************/
/* Constants used by SCPI Parser																											*/
/**************************************************************************************/

#define CMD_SEP									';'						/* Symbol used to separate commands within the Input String 						 */
#define CMD_ROOT								':'						/* Symbol used to tell SCPI parser to reset to root of Command Tree 		 */
#define KEYW_SEP								':'						/* Symbol used to separate keywords within a command 										 */
#define PARAM_SEP								','						/* Symbol used to separate parameters within the Input Parameters String */
#define SINGLE_QUOTE						'\''					/* Single quote symbol used to delimit quoted strings 									 */
#define DOUBLE_QUOTE						'"'						/* Double quote symbol used to delimit quoted strings 									 */
#define OPEN_BRACKET						'('						/* Symbol used as opening bracket at start of an Expression parameter 	 */
#define CLOSE_BRACKET						')'						/* Symbol used as closing bracket at end of an Expression parameter 	 	 */
#define ENTRY_SEP								','						/* Symbol used to separate entries in a numeric list or a channel list 	 */
#define RANGE_SEP								':'						/* Symbol used to separate start and finish values of an entry in a list */
#define DIM_SEP									'!'						/* Symbol used to separate mulit-dimensioned entries in a list					 */
#define NUM_SUF_SYMBOL					'#'						/* Symbol used to represent a numeric suffix in command keywords spec	   */
#define CMD_COMMON_START				'*'						/* Symbol used as first character of common command */

#define BOOL_ON									"ON"					/* Textual representation of the Boolean value ON  (1) */
#define BOOL_OFF								"OFF"					/* Textual representation of the Boolean value OFF (0) */
#define BOOL_ON_LEN							(2)
#define BOOL_OFF_LEN						(3)

#define MAX_EXPONENT						(43)					/* Largest value of exponent allowed  */
#define MIN_EXPONENT						(-43)					/* Smallest value of exponent allowed */

#define BASE_BIN								(2)						/* Number bases that can be used in SCPI */
#define BASE_OCT								(8)
#define BASE_DEC								(10)
#define BASE_HEX								(16)


/**************************************************************************************/
/******* Non-class member functions used by both SCPIParser & SCPIParam classes *******/
/**************************************************************************************/

#ifdef SUPPORT_CHAN_LIST
UCHAR ParseAsChannelRange(char *SParam, SCPI_CHAR_IDX *pPos, SCPI_CHAR_IDX End,
	BOOL bCheck, const struct strSpecAttrChanList *pSpecAttr);
#endif
#ifdef SUPPORT_CHAN_LIST_ADV
UCHAR ParseAsModuleChannel(char *SParam, SCPI_CHAR_IDX *pPos, SCPI_CHAR_IDX End,
	BOOL bCheck, const struct strSpecAttrChanList *pSpecAttr);
UCHAR ParseAsPathName(char *SParam, SCPI_CHAR_IDX *pPos, SCPI_CHAR_IDX End);
UCHAR ParseAsCharProgData(char *SStr, SCPI_CHAR_IDX Len);
#endif
#ifdef SUPPORT_NUM_SUFFIX
BOOL AppendToUInt(unsigned int *puiVal, char Digit);
#endif
BOOL AppendToULong(unsigned long *pulVal, char Digit, UCHAR Base);
BOOL AppendToInt(int *piVal, char Digit);
UCHAR cabs(signed char Val);
BOOL iswhitespace(char c);


/**************************************************************************************/
/* Substitutable Functions																														*/
/* -----------------------																														*/
/* Refer to text near end of module regarding these functions													*/
/**************************************************************************************/
SCPI_CHAR_IDX strlen(const char *S);
char tolower(char c);
BOOL islower(char c);
BOOL isdigit(char c);
#ifdef SUPPORT_CHAN_LIST_ADV
BOOL isalpha(char c);
BOOL isalnum(char c);
#endif


#endif	/* SCPIPRIV_H */

