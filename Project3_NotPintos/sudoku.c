/*Michael Carr and Gabrielle Luna*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define BUFFERSIZE 100

//Global variables for sudoku dimensions
int NumColumns = 9;
int NumRows = 9;

//Struct to pass between threads
typedef struct
{
  int row, col, isSolved;
  int ** grid;
}checkGrid;

/***Function Prototypes*/
/**/int ** ParseInput();
/**/checkGrid * setGridParams(int i, int ** grid);
/**/void * CheckRow (void * args);
/**/void * CheckColumn(void * args);
/**/void * CheckSquare(void * args);

/******************************************************/
/*main: DRIVER                                        */
/******************************************************/

int main (int argc, char * argv[])
{
  //Ensure proper usage
  if (argc != 1)
  {
    printf("Error\nUsage: ./sudoku.x < someInputFile\n");
    return 1;
  }
  printf("\nSudoku Results: \n\n");

  //Parse input file
  int ** sudokuGrid = ParseInput();

  //Set up parameters for grid checking
  checkGrid * check = (checkGrid *) malloc(sizeof(checkGrid));
  checkGrid * checkSquare = (checkGrid * ) malloc (sizeof(checkGrid));
  
  //Create threads
  pthread_t t_rows, t_columns, t_squares;

  //Return Values for threads
  void * rowsReturn;
  void * columnsReturn;
  void * squaresReturn;
  
  int IsSolved = 1;

  //start row aand column check threads
  check = setGridParams(0, sudokuGrid);
  for (int i = 0; i < NumRows; ++i)
    {
      //Set necessary params
      check->row = i;
      check->col = i;
      checkSquare = setGridParams(i + 1, sudokuGrid);
      
      pthread_create(&t_rows, NULL, CheckRow, (void *) check);
      pthread_create(&t_columns, NULL, CheckColumn, (void *) check);
      pthread_create(&t_squares,NULL, CheckSquare, (void *) checkSquare);
      
      pthread_join(t_rows, &rowsReturn);
      pthread_join(t_columns, &columnsReturn);
      pthread_join(t_squares, &squaresReturn);

    }
  if (check->isSolved == 0)
    IsSolved = 0;
  if (checkSquare->isSolved == 0)
    IsSolved = 0;

  printf("\n");


  if (IsSolved == 1)
    printf("The input is valid Sudoku\n\n");
  else
    printf("The input is not valid Sudoku \n\n");
  

  return 0;
}

/******************************************************/
/*ParseInput: 
/******************************************************/
int ** ParseInput()
{
  int ** sudokuGrid = 0;
  char buffer[BUFFERSIZE];
  char c;
  int num, col;

  //Allocate rows
  sudokuGrid = malloc(sizeof(int *) * NumRows);
  
  //Insert data into grid one row at a time
  for (int row = 0; row < NumRows; ++row)
    {
      //Allocate columns
      sudokuGrid[row] = malloc(sizeof(int *) * NumColumns);
      col = 0;
      
      //Grab line from input file
      fgets(buffer, BUFFERSIZE, stdin);
      //      printf("buffer: %s\n", buffer);
      
      //Iterate through buffer
      for (int i = 0; i < strlen(buffer); ++i)
	{
	  c = buffer[i];
	  //Only take digits
	  if (isdigit(c))
	    {
	      num = atoi(&c);
	      num = num / 10;
	      sudokuGrid[row][col] = num;
	      if (col < NumColumns)
		col++;
	    }
	}
      if(sudokuGrid[row][col - 1] == 0)
	row--;
    }

  return sudokuGrid;
}

/******************************************************/
/*checkGridParams:
/******************************************************/
checkGrid * setGridParams(int i, int ** sudokuGrid)
{
  checkGrid * check = (checkGrid * )malloc(sizeof(checkGrid));
  check->row = 0;
  check->col = 0;
  check->grid = sudokuGrid;

  //used to set row and column markers for checking boxes
  //Boxes numberes left to right, top to bottom
  if (i == 2)
    check->col = 3;
  if (i == 3)
    check->col = 6;
  if (i == 4)
    check->row = 3;
  if (i == 5)
    {
      check->row = 3;
      check->col = 3;
    }
  if (i == 6)
    {
      check->row = 3;
      check->col = 6;
    }
  if (i == 7)
      check->row = 6;
  if (i == 8)
    {
      check->row = 6;
      check->col = 3;
    }
  if(i == 9)
    {
      check->row = 6;
      check->col = 6;
    }

  return check;
}

/******************************************************/
/*CheckRows:
/******************************************************/
void * CheckRow(void * args)
{
  checkGrid * check = (checkGrid *) args;
  int numCheck[10] = {0};

  //Iterate through columns in row
  for (int j = 0; j < NumColumns; ++j)
    {
      int num = check->grid[check->row][j];
      //If we have already seen number
      if (numCheck[num])
	{
	  printf ("Row %d doesn't have the required values.\n", check->row + 1);
	  check->isSolved = 0;
	}
      else
	//Mark number as seen
	numCheck[num] = 1;
    }
  pthread_exit(1);
}

/******************************************************/
/*CheckRows: 
/******************************************************/
void * CheckColumn(void * args)
{
  checkGrid * check = (checkGrid *) args;
  int numCheck[10] = {0};

  //Iterate through rows in column
  for (int j = 0; j < NumRows; ++j)
    {
      int num = check->grid[j][check->col];
      if (numCheck[num])
	{
	  //If we have already seen number
	  printf ("Column %d doesn't have the required values.\n", check->col + 1);
	  check->isSolved = 0;
	}
      else
	//Mark number as seen
	numCheck[num] = 1;
    }
  pthread_exit(1);
}

/******************************************************/
/*CheckSquare:
/******************************************************/
void * CheckSquare(void * args)
{
  checkGrid * check = (checkGrid *) args;
  int numCheck[10] = {0};

  int rowStart = check->row;
  int colStart = check->col;

  for (int i = rowStart; i < rowStart + 3; i ++)
    {
      for (int j = colStart; j < colStart + 3; j++)
	{
	  int num = check->grid[i][j];
	  if(numCheck[num])
	    {
	      int m = 0;
	      //If we have already seen number
	      if (j <3)
		printf ("The left");
	      else if (j < 6)
		{
		  printf("The middle");
		  m = 1;
		}
	      else
		printf("The right");
	      if(i < 3)
		printf(" top");
	      else if (i < 6 && !m )
		printf(" middle");
	      else if (!m)
		printf(" botom");
	      printf(" subgrid doesn't have the required values.\n");	  
	      check->isSolved = 0;
	    }
	  else
	    //Mark number as seen
	    numCheck[num] = 1;
	}
    }
  pthread_exit(1);
}

/******************************************************/
/******************************************************/
/******************************************************/
/******************************************************/
/******************************************************/
/******************************************************/
/******************************************************/
/******************************************************/
/******************************************************/
