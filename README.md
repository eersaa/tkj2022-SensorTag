# Sensor tag project

To import the project to Code Composer Studio:
- Clone the repository to somewhere
- Open the CCS and choose File..Switch workspace
- Navigate to cloned repository folder and open it as workspace
- Click Project and select Import project
- Navigate to project folder and import
- Voilá

# Mpu9250 simulator program

To compile the project go to src folder and run (on Linux):

```
make
```
Commands in makefile compiles the library and main.

You can use the launch.json configuration with vscode to debug the program. Or you can run the program from command line. For now program needs to be inside the src folder to program to be able to find the csv data files from misc folder.
