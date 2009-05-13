#include <stdio.h>
#include <stdlib.h>

#include <stdbool.h>
#include <string.h>

#include <assert.h>
#include <ctype.h>


// Local headers

#include "macro_vm.h"
#include "gerber_parse.h"
#include "fileio.h"
#include "main.h"
// Overall application layout
// Scanner converts gerber into gerber file /w params for single-set-things
//  + rejects obviously bad gerbers
//
// Interpreter runs the gerber code to create a set of vectors defining paths
// Stores in quadtree
// Also takes overlapping vectors to mean single signal - IPC365 helpful here?
//
// DRC takes interpreted signals - checks spacing.








/********************************************************/
/* 274X parse code                                      */
/********************************************************/

// We track if these are set because that way we can catch
//  malformed gerbers


// Initialize the image params to the values specified by the standard
void initialize_rs274x_image_param_struct(struct rs274x_image_param * ip)
{
	// Image Polarity defaults to POS
	ip->IP_set = false;
	ip->IP_polarity = IP_POS;

	// I don't have information on how the image justify is initialized
	ip->IJ_set = false;	
	ip->IJ_offset_A = 0;
	ip->IJ_offset_B = 0;

	// Image rotate, again, probably never used, but completeness
	ip->IR_set = false;
	ip->IR_set = IR_0;
	
	// Plotter film is probably going to be unused - but I'll fill it in
	// for sake of completion
	ip->PF_set = false;
	ip->PF_value = NULL;

	// Image Name
	ip->IN_set = false;
	ip->IN_value = NULL;

	// Image Offset
	ip->IO_set = false;
	ip->IO_offset_A = 0.0;
	ip->IO_offset_B = 0.0;
}

struct aperture * create_aperture()
{
	struct aperture * rv = (struct aperture *)calloc(sizeof(struct aperture),1);
	return rv;
}



// TODO: These seem like reasonale defaults
// Ask SF what they see
void initialize_parse_info(struct parse_info *pi)
{
	pi->parse_set = false;
	pi->lt = OMIT_LEADING;
	pi->ai = COORD_ABS;
	pi->N_width = 2;
	pi->G_width = 2;
	pi->D_width = 3;
	pi->M_width = 2;
	pi->X_lead = 2;
	pi->X_trail = 3;
	pi->Y_lead = 2;
	pi->Y_trail = 3;
}





struct gerber_file * create_and_init_gerber_rep()
{
	struct gerber_file * file_rep = (struct gerber_file *)calloc(sizeof(struct gerber_file), 1);
	initialize_rs274x_image_param_struct(&(file_rep->image_params));
	initialize_parse_info(&(file_rep->parse_settings));

	file_rep->ap_map = std::map<std::string, Macro_VM *>();
	file_rep->first_gcode = NULL;
	file_rep->last_gcode = NULL;
	return file_rep;
}

void append_gcode_block(struct gerber_file * gerb, struct gcode_block * blk)
{
	blk->next = NULL;
	if (gerb->last_gcode != NULL)
	{
		assert(gerb->last_gcode->next == NULL);
		gerb->last_gcode->next = blk;
		gerb->last_gcode = blk;
	} else {
		gerb->last_gcode = blk;
		gerb->first_gcode = blk;
	}
}

#define INTPREF(a) ((a)[0] << 8) | ((a)[1])
#define INTPM(a,b) ((a) << 8) | (b)

// Returns a list of all paramaters from starptr, less than endptr separated by char DELIM's

void free_delim_list(char**dlist)
{
	char ** iter = dlist;
	while ((*iter) != NULL) 
	{
		free(*iter);
		iter++;
	}
	free(dlist);
}

char ** breakout_delim(char * startptr, char * endptr, char delim, int * cr)
{
	//printf("Attempting to breakout %s %s [%c]\n", startptr, endptr, delim);
	
	char * iter;
	long count = 1;
	long block_iter;
	
	// Ok, we have at least one block
	if (endptr - startptr == 0)
		return NULL;
	
	for (iter = startptr; iter < endptr; iter++)
	{
		if (*iter == delim)
			count ++;
	}
	
	//printf("Found %d blocks\n", count);

	char ** rv = (char **)malloc(sizeof(char*)*(count + 1));
	
	rv[count] = 0;

	iter = startptr;
	for (block_iter = 0; block_iter < count; block_iter++)
	{	
		assert(iter < endptr);
		char * blockend = strchr(iter, delim);
		if (blockend == NULL)
			blockend = endptr;
		
		rv[block_iter] = strndup(iter, blockend - iter);
	//	printf("Block = %s\n",rv[block_iter]);
		iter = blockend + 1;

		
	}

	if (cr != NULL)
		*cr = count;
	
	return rv;
}


/************************************************************/
/* Stubs for image information - when this becomes a general
 * purpose parser - I'll get around to implementing these.
 * but for the sparkfun DRC, I don't need them
 * **********************************************************/
bool handle_274X_IJ(char * block, struct gerber_file * target)
{
	assert((block[0] == 'I') && (block[1] == 'J'));

	// TODO: This is probably unimportant for DRC - not implemented yet
	return true;
}

bool handle_274X_IN(char * block, struct gerber_file * target)
{
	assert((block[0] == 'I') && (block[1] == 'N'));

	// TODO: This is probably unimportant for DRC - not implemented yet
	return true;
}

bool handle_274X_IO(char * block, struct gerber_file * target)
{
	assert((block[0] == 'I') && (block[1] == 'O'));

	// TODO: This is probably unimportant for DRC - not implemented yet
	return true;
}

bool handle_274X_IP(char * block, struct gerber_file * target)
{
	assert((block[0] == 'I') && (block[1] == 'P'));

	// TODO: This is probably unimportant for DRC - not implemented yet
	return true;
}

bool handle_274X_IR(char * block, struct gerber_file * target)
{
	assert((block[0] == 'I') && (block[1] == 'R'));

	// TODO: This is probably unimportant for DRC - not implemented yet
	return true;
}

bool handle_274X_PF(char * block, struct gerber_file * target)
{
	assert((block[0] == 'P') && (block[1] == 'F'));

	// TODO: This is probably unimportant for DRC - not implemented yet
	return true;
}

/*****************************************************************************
 * aperture handling code - right now, only AD handled, but AM goes in
 * here as well
 * ***************************************************************************/

bool handle_274X_AD(char * block, struct gerber_file * target)
{
	assert((block[0] == 'A') && (block[1] == 'D'));
	
	
	
	// ensure its defining a proper DCODE value
	if (block[2] != 'D')
	{
		DBG_VERBOSE_PF("Inval aperture define value\n");
		// HACK: I hate people. This is completely invalid, yet some gerber software spits out
		// things like %AD*% - and we have to parse it. Argh.
		return true;
	}

	char * parseptr;
	// 3 is the offset of the DCODE ID
	long ap_num = strtol(block + 3, &parseptr, 10);

	if (block + 3 == parseptr)
	{
		if (debug_level == DEBUG_ERROR)
			printf("0 length DCODE ID\n");
		
		return false;
	}

	if ((ap_num < 10) || (ap_num >= MAX_APERTURES))
	{
		if (debug_level == DEBUG_ERROR)
			printf("Invalid DCODE ID %d\n", ap_num);
		return false;
	}

	
	char * end_typestr = strchr(parseptr, ',');

	int typestr_len;
	if (end_typestr == NULL)
		typestr_len = strlen(parseptr);
	else
		typestr_len = end_typestr - parseptr;
	
	struct aperture * ap;

	if (target->ap_list[ap_num] != NULL)
	{
		if (debug_level >= DEBUG_ERROR)
			printf( "Error - attempted to redefine an aperture.\n"
				" [This is technically allowed, but not parsed]\n");
		return false;
	}
	// Check if its one of the standard primitive types - Typestring Length 1 & typestring ~= [CROPT]
	if (typestr_len == 1 && ((parseptr[0] == 'C') || (parseptr[0] == 'R') || 
		(parseptr[0] == 'O') || (parseptr[0] == 'P') || (parseptr[0] == 'T')))	
	{
		char ap_type = parseptr[0];
		parseptr += typestr_len;
		

		char ** arg_list = NULL;

		int param_count = 0;

		if (strlen(parseptr) >= 1)
		{
			parseptr++;
			arg_list = breakout_delim(parseptr, strlen(parseptr)+parseptr, 'X', &param_count);
		}

		// Circle args [3] - OD, [IX, IY]
		// Rect args [4] - OX, OY, [IX, IY]
		// Oval args [4] - OX, OY, [IX, IY]
		// Poly args [5] - OD, NS, [DR, IX, IY]

		if (debug_level >= DEBUG_MSG)
			printf("aperture define %d %c [%s]\n", ap_num,ap_type, block);
	
		// now allocate a new aperture
		ap = create_aperture();

		
		switch (ap_type)
		{
			case 'C':
				if (param_count < 1)
				{
					if (debug_level >= DEBUG_ERROR)
						printf("Not enough args for circle aperture\n");
					return false;
				}
				ap->type = AP_CIRCLE;
				ap->circle_p.OD = atof(arg_list[0]);

				if (param_count > 1)
					ap->circle_p.XAHD = atof(arg_list[1]);
				
				if (param_count > 2)
					ap->circle_p.YAHD = atof(arg_list[2]);

				break;
			case 'R':
				if (param_count < 2)
				{
					if (debug_level >= DEBUG_ERROR)
						printf("Not enough args for rect aperture\n");
					return false;
				}
				ap->type = AP_RECT;
				ap->rect_p.XAD = atof(arg_list[0]);
				ap->rect_p.YAD = atof(arg_list[1]);				
				if (param_count > 2)
					ap->rect_p.XAHD = atof(arg_list[1]);
				
				if (param_count > 3)
					ap->rect_p.YAHD = atof(arg_list[2]);
				
				break;

			case 'O':
				if (param_count < 2)
				{
					if (debug_level >= DEBUG_ERROR)
						printf("Not enough args for oval aperture\n");
					return false;
				}
				ap->type = AP_OVAL;
				ap->oval_p.XAD = atof(arg_list[0]);
				ap->oval_p.YAD = atof(arg_list[1]);				
				if (param_count > 2)
					ap->oval_p.XAHD = atof(arg_list[1]);
				
				if (param_count > 3)
					ap->oval_p.YAHD = atof(arg_list[2]);
				break;
				
			case 'P':
				if (param_count < 2)
				{
					if (debug_level >= DEBUG_ERROR)
						printf("Not enough args for poly aperture\n");
					return false;
				}
				ap->type = AP_POLY;

				ap->poly_p.OD = atof(arg_list[0]);
				ap->poly_p.NS = atoi(arg_list[1]);

				if (param_count > 2)
					ap->poly_p.DR = atoi(arg_list[2]);
				
				if (param_count > 3)
					ap->poly_p.XAHD = atoi(arg_list[3]);
				
				if (param_count > 4)
					ap->poly_p.YAHD = atoi(arg_list[4]);
				break;
				
			case 'T':
				
				if (debug_level >= DEBUG_ERROR)
					printf("Error - ap type T not yet supported\n");
				return false;	
		}
		if (arg_list)
			free_delim_list(arg_list);
	} else {
		// Check if it exists in the macro list
		// However - right now we can't parse macros
		if (debug_level >= DEBUG_MSG)
			printf("aperture define %d MACRO [%s]\n", ap_num, block);
		
		ap = create_aperture();
		ap->type = AP_MACRO;
		ap->macro_p.macro_name = strndup(parseptr, typestr_len-1);
		ap->macro_p.compiled_macro = target->ap_map[ap->macro_p.macro_name];
		
		parseptr += typestr_len;
		

		char ** arg_list = NULL;

		int param_count = 0;

		if (strlen(parseptr) >= 1)
		{
			parseptr++;
			arg_list = breakout_delim(parseptr, strlen(parseptr)+parseptr, 'X', &param_count);
		
			double * args = (double*)malloc(sizeof(double)*param_count);
			 
			char ** afollow = arg_list;
			int i=0;
			while (*afollow != NULL)
			{
				printf("Arg: %s\n", *afollow);
				args[i++] = atof(*afollow);
				afollow ++;
			}
			
			if (arg_list)
				free_delim_list(arg_list);
			
			ap->macro_p.params = args;
		} else {
			ap->macro_p.params = NULL;
		}
		printf("Looking up macro %s [%p]\n",ap->macro_p.macro_name, ap->macro_p.compiled_macro);
	}
	
	target->ap_list[ap_num] = ap;

	return true;	
}

/************************************************************************/
/* DIRECTIVE parsing code ***********************************************/
/************************************************************************/

/* FS */
bool handle_274X_FS(char * block, struct gerber_file * target)
{
	assert((block[0] == 'F') && (block[1] == 'S'));
	
	char * blockend = block + strlen(block);

	// skip the block ID
	block += 2;
	//printf("%20s\n",block);
	switch (*block) {
		case 'L':
			target->parse_settings.lt = OMIT_LEADING;
			break;
		case 'T':
			target->parse_settings.lt = OMIT_TRAILING;
			break;
		default:
			if (debug_level >= DEBUG_ERROR)
			{
				printf("Unrecognized Lead / Trail selector %c in FS\n", *block);
				return false;
			}

	}
	block++;

	switch (*block) {
		case 'A':
			target->parse_settings.ai = COORD_ABS;
			break;
		case 'I':
			target->parse_settings.ai = COORD_INC;
			break;
		default:
			if (debug_level >= DEBUG_ERROR)
				printf("Unrecognized ABS/INC selector %c in FS\n", *block);

	}
	block++;

	// Check if we're going to specify the N width
	if (*block == 'N')
	{
		char * numend;
		*block++;
		target->parse_settings.N_width = strtol(block, &numend, 10);
		if (numend == block)
		{
			if (debug_level >= DEBUG_ERROR)
				printf("Could not parse Nwidth in FS\n");
			return false;
		}

		block = numend;
	}
	
	// Check if we're going to specify the G width
	if (*block == 'G')
	{
		char * numend;
		*block++;
		target->parse_settings.G_width = strtol(block, &numend, 10);
		if (numend == block)
		{
			if (debug_level >= DEBUG_ERROR)
				printf("Could not parse Gwidth in FS\n");
			return false;
		}

		block = numend;
	}
	
	if (*block == 'X')
	{
		// Skip the X
		*block++;

		if ((*block > '6') || (*block < '0'))
		{
			if (debug_level >= DEBUG_ERROR)
				printf("Error - invalid X lead specifier [%c] in FS\n",*block);
					
			return false;
		}
		target->parse_settings.X_lead = (*block) - '0';	

		// Go to the second digit of X spec
		*block++;

		if ((*block > '6') || (*block < '0'))
		{
			if (debug_level >= DEBUG_ERROR)
				printf("Error - invalid X trail specifier [%c] in FS\n",*block);
					
			return false;
		}
		target->parse_settings.X_trail = (*block) - '0';	

		//printf("X_lead = %d\n",target->parse_settings.X_lead);
		//printf("X_trail = %d\n",target->parse_settings.X_trail);
		// Go past the last digit
		*block++;
	}
	
	if (*block == 'Y')
	{
		// Skip the Y
		*block++;

		if ((*block > '6') || (*block < '0'))
		{
			if (debug_level >= DEBUG_ERROR)
				printf("Error - invalid Y lead specifier [%c] in FS\n",*block);
					
			return false;
		}
		target->parse_settings.Y_lead = (*block) - '0';	

		// Go to the second digit of Y spec
		*block++;

		if ((*block > '6') || (*block < '0'))
		{
			if (debug_level >= DEBUG_ERROR)
				printf("Error - invalid Y trail specifier [%c] in FS\n",*block);
					
			return false;
		}
		target->parse_settings.Y_trail = (*block) - '0';	


		//printf("Y_lead = %d\n",target->parse_settings.Y_lead);
		//printf("Y_trail = %d\n",target->parse_settings.Y_trail);
		// Go past the last digit
		*block++;
	}

	
	// Check if we're going to specify the D width
	if (*block == 'D')
	{
		char * numend;
		*block++;
		target->parse_settings.D_width = strtol(block, &numend, 10);
		if (numend == block)
		{
			if (debug_level >= DEBUG_ERROR)
				printf("Could not parse Dwidth in FS\n");
			return false;
		}

		block = numend;
	}
	
	// Check if we're going to specify the M width
	if (*block == 'M')
	{
		char * numend;
		*block++;
		target->parse_settings.M_width = strtol(block, &numend, 10);
		if (numend == block)
		{
			if (debug_level >= DEBUG_ERROR)
				printf("Could not parse Mwidth in FS\n");
			return false;
		}

		block = numend;
	}

	
	target->parse_settings.parse_set = true;
	return true;
}

/* OF - Offset */
/* MO - Mode */
bool handle_274X_MO(char * block, struct gerber_file * target)
{
	assert((block[0] == 'M') && (block[1] == 'O'));
	block += 2;
	enum unit_mode um;

	if ((block[0] == 'M') && (block[1] == 'M'))
	{
		
		um = UNITMODE_MM;
	} else if ((block[0] == 'I') && (block[1] == 'N'))
	{
		um = UNITMODE_IN;
	} else {

		if (debug_level >= DEBUG_ERROR)
			printf("Error - invalid Mode line\n");

		return false;
	}

	struct gcode_block * eob = (struct gcode_block *)malloc(sizeof(struct gcode_block));
	eob->op = GCO_DIR;
	eob->gdd_data.dir = DIR_MO;
	eob->gdd_data.MO_P.um = um;
	append_gcode_block(target, eob);

	return true;
}

/*
 * Aperture M
 *
 *
 *
 */
bool handle_274X_AM(char ** cur_block_ptr, struct gerber_file * target, int * consumed)
{
	*consumed = 0;
	char * macroName = strdup(*cur_block_ptr + 2);
	Macro_VM * vm = new Macro_VM();
	
	printf("Macro Name = %s\n",macroName);
	*cur_block_ptr ++;
	(*consumed)++;
	
	while (*cur_block_ptr != NULL)
	{
			(*consumed)++;
			printf("Macro Consuming %s [%d]\n", *cur_block_ptr);
			if (parse_macro(vm,*cur_block_ptr))
				printf("Parse_OK\n");
			else
			{
				printf("Parse Failed!!!!\n");
				delete vm;
				free(macroName);
				return false;
			}
				
			cur_block_ptr++;
	}
	std::string name_std(macroName);
	target->ap_map[name_std] = vm;
	free(macroName);

	return true;
}
/* General format for a param block
 *
   %PARAM1*
   PARAM2*
   PARAM3*
   MACRO*
   MACROSETTINGS*
   %

   [Some programs seem to add that newline :/]
   
   Parsing Approach - get the bounds on the %'s
   Tokenize it into blocks and discard empty blocks [created by eagle :/] 
   [ linkedlist isn't needed, just an array - count the *'s to get a count of blocks]
   Last element NULL;
    aka:
   PARAM1
   PARAM2
   PARAM3
   MACRO
   MACROSETTINGS
   NULL

   Pass each interpeter a handle, let them consume as many blocks as they need
 *
 */
 
bool parse_274X_param_do_block(char ** cur_block_ptr, struct gerber_file * target, int * consumed)
{
	bool parse_ok = true;
	
	// for the most case, everything consumes a block
	*consumed = 1;
	//printf("BLOCK %c %c\n", **cur_block_ptr, *(*cur_block_ptr+1));
	fflush(NULL);
	// Ugly define macros for nice speedy [and clean looking!] switch statements
	switch (INTPREF(*cur_block_ptr))
	{

		// Plotter parameters - these are all stubs as of right now
		case INTPM('I','J'):
			parse_ok = handle_274X_IJ(*cur_block_ptr, target);
			break;
		case INTPM('I','N'):
			parse_ok = handle_274X_IN(*cur_block_ptr, target);
			break;
		case INTPM('I','O'):
			parse_ok = handle_274X_IO(*cur_block_ptr, target);
			break;
		case INTPM('I','P'):
			parse_ok = handle_274X_IP(*cur_block_ptr, target);
			break;
		case INTPM('I','R'):
			parse_ok = handle_274X_IR(*cur_block_ptr, target);
			break;
		case INTPM('P','F'):
			parse_ok = handle_274X_PF(*cur_block_ptr, target);
			break;


		// Directives - these are important
		case INTPM('F','S'):
			parse_ok = handle_274X_FS(*cur_block_ptr, target);
			break;	

		case INTPM('M','O'):
			parse_ok = handle_274X_MO(*cur_block_ptr, target);
			break;	



		case INTPM('A','D'):
			parse_ok = handle_274X_AD(*cur_block_ptr, target);
			break;
			
		case INTPM('A','M'):
			parse_ok = handle_274X_AM(cur_block_ptr, target, consumed);
			break;
			
		case INTPM('L','P'):
			if ((*cur_block_ptr)[2]!='D')
			{
				//printf("Cannot handle inverted polarity layers!!!\n");
				//parse_ok = false;
			}
			break;
			
		default:
			printf("unmatched 274X paramblock %s - len %d\n", *cur_block_ptr, strlen(*cur_block_ptr));
	}
	
	return parse_ok;
	
	//if (
	//cur_block_ptr++;

	// Parameters we need to handle:
	
	//
	// [. = stub; ~ = mostly there; * = finished;
	//
	// DIRECTIVES:
	// These are unfortunatly order sensitive - so we need to include them on the parse list
	// We might be able to get away with only checking these once
	//	 AS - Axis Select
	// *	 FS - Format Statement
	//	 MI - Miror Image
	//	 MO - Mode of Units
	//	 OF - Offset [ we can probably ignore offset... ]
	//	 SF - Scale Factor
	
	// IMAGE PARAMETERS:
	// We can store a single cached copy of these
	// .	 IJ
	// .	 IN
	// .	 IO
	// .	 IP
	// .	 IR
	// .	 PF
	
	// APERTURE PARAMETERS:
	// ~	 AD [ Can define a new layer. Blah ]
	//	 AM [this one is tricky]
	
	// LAYER Creating Params
	//	 KO
	//	 LN
	//	 LP
	//	 SR
	
	//	 MISC PARAMETERS:
	//	 IF - WE WILL NOT SUPPORT THIS PARAM. That would mean I need to make all my code recursive :(
		
}

bool parse_274X_param(char ** cur_ptr, char * end_ptr, struct gerber_file * target)
{
	// Make sure that we're actually in a formal parameter
	// This assert should always be true - just a safety net
	assert(**cur_ptr == '%');

	// Search for our end of block character
	char * end_char = (char *)memchr((*cur_ptr) + 1, '%', end_ptr - ((*cur_ptr) + 1) + 1);

	// If there isn't an end of block pattern
	if (end_char == NULL)
	{
		if (debug_level >= DEBUG_ERROR)
			printf("Unmatched %% in 274X param");

		return false;
	}
	
	// Handy pointer to our first char
	char * first_param_char = (*cur_ptr) + 1;
	// Length of 274X param
	int param_len = (end_char-1) - first_param_char + 1;
	
	//printf("%s\n",strndup(first_param_char,param_len));

	
	// Now we find how many blocks there are inside this param
	long block_count = 0;
	char * param_iter;
	for (param_iter = first_param_char; param_iter < end_char; param_iter++)
	{
		if (*param_iter == '*')
			block_count++;
	}

	if (debug_level == DEBUG_VERBOSE)
		printf("Block count = %d\n", block_count);
	if (block_count == 0)
		block_count = 1;
	char **pblocks = (char**)malloc(sizeof(char *) * (block_count + 1));
	// Last one is an end of list flag
	pblocks[block_count] = NULL;


	long block_iter;
	param_iter = first_param_char;
	for (block_iter = 0; block_iter < block_count; block_iter++)
	{	
		// Find the next *
		// Not inclusive to end_ptr as end_ptr is just a % - or it should be
		// if we're already here ;)
		char * end_of_block = (char*)memchr(param_iter, '*', end_ptr - param_iter);
		
		// Now we create a string up to the end of that block
		// Noninclusive of end_of_block - because we don't want * at the end
		pblocks[block_iter] = strndup(param_iter, end_of_block - param_iter - 1);

		// Seek our iterater to just past the end of this block
		param_iter = end_of_block + 1;

		// We also want to skip any return characters / whitespace
		while (isspace(*param_iter) && param_iter < end_ptr) param_iter++;
		
		if (debug_level == DEBUG_VERBOSE)
			printf("Block %d = %s\n", block_iter, pblocks[block_iter]);

		assert (param_iter < end_ptr);
	}
	
	
	
	/* This odd little construct here allows a parser to consume more than one
	 * param block - only the macro param needs this */

	char ** cur_block_ptr = pblocks;
	// For every block we detected
	while (*cur_block_ptr != NULL)
	{
		int consumed;
		// Parse it
		if (!parse_274X_param_do_block(cur_block_ptr, target, &consumed))
		{
			return false;
		}
	
		// and skip to the next block
		cur_block_ptr+=consumed;
	}
	
	// Seek the pointer past how much we've consumed
	*cur_ptr = end_char + 1;

	for (block_iter =0; block_iter < block_count; block_iter++)
		free(pblocks[block_iter]);

	free(pblocks);
	return true;
}


/**************************************************************************/
/**************************************************************************/
/*   274 D parameter parsing Code					  *
 *   									  */
/**************************************************************************/
/**************************************************************************/
bool handle_274D_G(int code, struct gerber_file * target)
{
	switch (code)
	{
		case  0: // Move			- Pass
		case  1: // Linear Interpolation	- Pass
		case  2: // Clockwise Interpolation	- Pass
		case  3: // Counterclockwise Interp	- Pass
		case 10: // Linear Interpolation 10X	- Pass
		case 11: // Linear Interpolation 0.1X	- Pass
		case 12: // Linear Interpolation 0.01X  - Pass
		case 36: // Polygon Fill On		- Pass
		case 37: // Polygon Fill Off		- Pass
		case 54: // Tool prepare		- Pass
		case 74: // Disable 360 Interp		- Pass
		case 75: // Enable 360 Interp		- Pass		
		case 70: // Inches			- Pass
		case 71: // MM				- Pass
		case 90: // Abs				- Pass
		case 91: // Inc				- Pass
		{
			struct gcode_block * eob = (struct gcode_block *)malloc(sizeof(struct gcode_block));
			eob->op = GCO_G;
			eob->int_data = code;
			append_gcode_block(target, eob);
			break;
		}

		
		// Nobody cares if we prepare to flash
		case 55:
			return true;	
		default:
			return false;
	}
	return true;
}

// Returns the parsed coord [native form - uncorrected for mm / inches]
bool parse_274D_coord(char ** coord_data_start, double * retval, char axis, struct parse_info * pi)
{
	int lead, trail;

	assert(axis == 'X' || axis == 'Y');

	lead = (axis == 'X') ? pi->X_lead:pi->Y_lead;
	trail = (axis == 'X') ? pi->X_trail:pi->Y_trail;


	bool is_pos = true;

	bool hit_numeric = false;

	bool parse_finished = false;

	double digit_buf = 0;
	int digit_count = 0;
	char * coord_data = *coord_data_start;
	
	
	while (true)
	{

		if (isspace(*coord_data))
		{
			coord_data++;
			continue;
		}

		if (isdigit(*coord_data))
		{
			
			digit_count++;
			digit_buf *= 10.0;
			digit_buf += *coord_data - '0';
			
			hit_numeric = true;
			coord_data++;

			continue;
		}

		if ((*coord_data == '+') ||(*coord_data == '-'))
		{
			if (hit_numeric)
			{
				if (debug_level >= DEBUG_ERROR)
					printf("Sign constant in middle of numeric constant");
				return false;
			}

			if (*coord_data == '-')
				is_pos = false;
			
			coord_data++;
			continue;
		}

		break;	
	}

	if (pi->lt == OMIT_TRAILING)
	{
		// we need to check if we need to scale the value by the missing digits
		
		while (digit_count < lead+trail)
		{
			digit_buf *= 10.0;
			digit_count++;
		}
	}

	if (!is_pos)
		digit_buf *= -1;
	
	while (trail > 0)
	{
		trail--;
		digit_buf = digit_buf / 10.0;
	}
	
	*retval = digit_buf;
	*coord_data_start = coord_data;
	return true;
}

bool parse_274D_command_word(char ** cur_ptr, char * end_ptr, struct gerber_file * target, bool * finish_parse)
{
	// We don't include end_ptr because its off the end
	int remain_len = end_ptr - *cur_ptr;

	char * end_of_block = (char*) memchr(*cur_ptr, '*', remain_len);
	
	if (end_of_block == NULL)
	{
		if (debug_level >= DEBUG_ERROR)
			printf("ERROR - tried to parse unterminated GCODE block\n");
		return false;
	}
	
	remain_len = end_of_block - *cur_ptr;
	

	// Things we might see within a block:
	//  G?? codes, D?? codes, X?..?, Y?..?, I?..?, J?..?, M??
	
	char * temp_ptr = *cur_ptr;
	bool datablock_ignore = false;

	int remain_iter = remain_len + 1;
	while ((temp_ptr < end_of_block) && !datablock_ignore)
	{
		if (remain_iter-- == 0)
		{
			printf("PARSE HANG - report this bug and send a test case!\n");
			return false;
		}

		switch (*temp_ptr)
		{
			// Some GCODE files improperly include spaces
			case ' ':
			case '\t':
			case '\n':
			case '\r':
				temp_ptr++;
				break;

			// These are all handled the same with the exception of the dest
			// 
			case 'M':
			case 'D':
			case 'G':
				{
					char code_dest = *temp_ptr;
	
					temp_ptr++;
	
					char * error_loc;
					int val = strtol(temp_ptr, &error_loc, 10);
					if (temp_ptr == error_loc)
					{
						if (debug_level >= DEBUG_ERROR)
						{
							printf("Could not parse %c command value!\n",code_dest);
						}
						return false;
					}
					temp_ptr = error_loc;
					switch (code_dest)
					{
						case 'M':
							{
								struct gcode_block * b = (struct gcode_block *)
									malloc(sizeof(struct gcode_block));
								b->op = GCO_M;
								b->int_data = val;
								append_gcode_block(target, b);
							}
							break;
						case 'D':
							{
								struct gcode_block * b = (struct gcode_block *)
									malloc(sizeof(struct gcode_block));
								b->op = GCO_D;
								b->int_data = val;
								append_gcode_block(target, b);
							}
							break;
							
						case 'G':
							/* Some G-codes need to be handled specially:
							 * They don't go through to the interpreter, but 
							 * are more related to in-file formatting
							 */
							if (val == 4)
							{
								datablock_ignore = true;
								break;
							}
							// Helper stub will handle adding it to target,
							// returns false if it was an invalid gval
							if (!handle_274D_G(val, target))	
							{
								if (debug_level >= DEBUG_ERROR)
								{
									printf("G-%d is not a valid command!\n", val);
								}	
								return false;
							}
							break;			
					}
					break;
				}
			// These are handled similarly - with the exception of the destination
			case 'X':
			case 'Y':
			case 'I':
			case 'J':
				{
					char coord_dest = *temp_ptr;
					temp_ptr++;
					double rv;
					if (!parse_274D_coord(&temp_ptr, &rv, 
							((coord_dest == 'X') || (coord_dest == 'I')) ? 'X' : 'Y',
						       	&target->parse_settings))
					{
						if (debug_level >= DEBUG_ERROR)
							printf("Couldn't parse num constant!\n");
						return false;
					}
					
					struct gcode_block * b = (struct gcode_block *)
						malloc(sizeof(struct gcode_block));
					
					
					
					switch (coord_dest)
					{
						case 'X':
							b->op = GCO_X;
							break;
						case 'Y':
							b->op = GCO_Y;
							break;
						case 'I':
							b->op = GCO_I;
							break;
						case 'J':
							b->op = GCO_J;
							break;
					}	
	
					b->dbl_data = rv;
					append_gcode_block(target, b);
					break;
				}	
			default:
				if (debug_level >= DEBUG_ERROR)
					printf("Error - couldn't parse GCODE statement\n");
				return false;
			break;
		}
	}

	// Sometimes we need to ignore a datablock, so thats what this flag is for
	if (!datablock_ignore)
	{
		struct gcode_block * eob = (struct gcode_block *)malloc(sizeof(struct gcode_block));
		eob->op = GCO_END;
		append_gcode_block(target, eob);
	}


	// Advance the consumed data pointer to past the end of block ptr
	*cur_ptr = end_of_block + 1;
	return true;
}

// Handy function to eat + echo an unknown line
void skip_unknown_line(char ** cur_ptr, char * end_ptr)
{
	printf("Unknown line: ");
	while (((*cur_ptr) < end_ptr) && ((**cur_ptr) != 0xD) && ((**cur_ptr) != 0xA)) putchar(*(*cur_ptr)++);
	while (((*cur_ptr) < end_ptr) && (((**cur_ptr) == 0xD) || ((**cur_ptr) == 0xA))) *(*cur_ptr)++;
	printf("\n");
}


bool parse_gerb_mem_block(char * cur_ptr, char * end_ptr, struct gerber_file * file_rep)
{
	//printf("Starting parsing %p %p\n", cur_ptr, end_ptr);
	//printf("End_ptr def = %d\n", *(end_ptr -1));

	bool finish_parse = false;
	while (cur_ptr < end_ptr && !finish_parse)
	{
		bool parse_ok = true;
		// we're at the beginning of a line
		switch (*cur_ptr)
		{
			// EOL character - skip it.
			case 0xA:
			case 0xD:
			case ' ':
			case '\t':
				cur_ptr++;
				break;

			case '%':
				parse_ok = parse_274X_param(&cur_ptr, end_ptr, file_rep);
				break;

			default:
				parse_ok = parse_274D_command_word(&cur_ptr, end_ptr, file_rep, &finish_parse);
		}	

		if (!parse_ok)
		{
			
			return false;
		}
	}

	assert(cur_ptr == end_ptr);
	return true;
}

struct gerber_file * create_gerber_file_rep_from_filename(char * filename)
{
	struct mapped_file mfile = map_file(filename);

	if (!mfile.valid)
	{
		printf("Could not load file %s\n", filename);
		return NULL;
	}

	char * cur_ptr = (char*)mfile.dataptr;
	char * end_ptr = (char*)mfile.dataptr + mfile.file_len;
	
	struct gerber_file * file_rep = create_and_init_gerber_rep();

	bool parse_ok = parse_gerb_mem_block(cur_ptr, end_ptr, file_rep);
	
	unmap_file(&mfile);
	
	if (!parse_ok)
	{
		// TODO - there be memory leaks here. Fix them
		free(file_rep);
		return NULL;
	}

	return file_rep;

}

void free_aperture(struct aperture * ap)
{
	free(ap);
}
void free_gerber_file_rep(struct gerber_file * rep)
{
	for (int i=0; i<100; i++)
		if (rep->ap_list != NULL)
			free_aperture(rep->ap_list[i]);
	
	struct gcode_block * gptr = rep->first_gcode;
	while (gptr != NULL)
	{
		struct gcode_block * nextptr = gptr->next;
		free(gptr);
		
		gptr = nextptr;
	}
	free(rep);
}

