import os
import subprocess
import sys


project_root = os.path.abspath(sys.argv[0])[:-len("tests.py")]

modules = []
for file in os.listdir(project_root):
    if os.path.isdir(project_root + file):
        modules.append(os.path.abspath(project_root + file))

modules.remove(project_root+"cmake")
modules.remove(project_root+"build")
modules.remove(project_root+".git")
try:
    modules.remove(project_root+".vscode")
except:
    print(".vscode folder not found")


for module in modules:
    result = subprocess.run(
        ["python3", module+"/tests.py"], stdout=subprocess.PIPE)
    if result.returncode is not os.EX_OK:
        if "03-engine-loop" in module:
            print("Current docker image does not support X11")
            continue
        print(os.path.basename(module), "module test failed", sep=" ")
        exit(os.EX_OSERR)

exit(os.EX_OK)
