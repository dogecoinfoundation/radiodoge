using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace RadioDoge
{
    public partial class SerDogeSharp
    {
        private SPVNodeManager spv;
        private readonly string spvDefaultCommand = "-c -b -d -a \"D6JQ6C48u9yYYarubpzdn2tbfvEq12vqeY DBcR32NXYtFy6p4nzSrnVVyYLjR42VxvwR DGYrGxANmgjcoZ9xJWncHr6fuA6Y1ZQ56Y\" -p scan";

        private void ProcessSPVCommand(int commandValue)
        {
            SPVFunctions commandType = (SPVFunctions)commandValue;
            switch (commandType)
            {
                case SPVFunctions.StartSPV:
                    bool runInOwnWindow = true;
                    spv = new SPVNodeManager(runInOwnWindow, spvDefaultCommand);
                    bool runSuccess = spv.Start();
                    if (runSuccess && !runInOwnWindow)
                    {
                        runSuccess = spv.ExitOnUserInput();
                    }
                    if (!runSuccess)
                    {
                        Console.WriteLine("Error: SPV Node failure!");
                    }
                    break;
                case SPVFunctions.StopSPV:
                    // Check to make sure we actually setup the SPV node first
                    if (spv == null)
                    {
                        Console.WriteLine("ERROR: SPV Node was not setup or started yet!");
                        break;
                    }

                    // Now try stopping the node
                    if (spv.Stop())
                    {
                        Console.WriteLine("Successfully stopped SPV Node!\n");
                    }
                    else
                    {
                        Console.WriteLine("ERROR: Failed to stop SPV Node!");
                    }
                    break;
                default:
                    ConsoleHelper.WriteEmphasizedLine("Unknown command", ConsoleColor.Red);
                    break;
            }
        }

        private void PrintSPVModeHelp()
        {
            ConsoleHelper.WriteEmphasizedLine("Available SPV Mode Commands:", ConsoleColor.Magenta);
            foreach (int i in Enum.GetValues(typeof(SPVFunctions)))
            {
                ConsoleHelper.WriteEmphasizedLine($"{i}: {(SPVFunctions)i}", ConsoleColor.Cyan);
            }
            ConsoleHelper.WriteEmphasizedLine("Enter 'exit' to return to the mode selection screen or 'help' for available commands\n", ConsoleColor.Magenta);
        }
    }
}
