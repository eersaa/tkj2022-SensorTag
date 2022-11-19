# Sensor tag project

To import the project to Code Composer Studio:
- Clone the repository to somewhere
- Open the CCS and choose File..Switch workspace
- Navigate to cloned repository folder and open it as workspace
- Click Project and select Import project
- Navigate to project folder and import
- Voilá

# Mpu9250 simulator program

## Linux environment
To be able to compile project you need some compiler like `gcc`. You can test if you have it already installed, by running in terminal:

```console
gcc -v
```

If you don't get any sensible output you can install the gcc with following commands:

```console
sudo apt-get update
```
```console
sudo apt-get install build-essential gdb
```

To compile the project go to src folder and run (on Linux):

```console
make
```
Commands in makefile compiles the library and main. For example you can write your algorithm to main.

You can use the launch.json configuration with vscode to debug the program. Or you can run the program from command line. For now program needs to be inside the src folder to program to be able to find the csv data files from misc folder.
