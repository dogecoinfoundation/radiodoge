using System.Text;

namespace RadioDoge
{
    public partial class SerDogeSharp
    {
        private SPVNodeManager spv;
        private readonly string spvDefaultCommand = "-c -b -d -a \"D6JQ6C48u9yYYarubpzdn2tbfvEq12vqeY DBcR32NXYtFy6p4nzSrnVVyYLjR42VxvwR DGYrGxANmgjcoZ9xJWncHr6fuA6Y1ZQ56Y\" -p scan";

        private void SendTestCommand(int commandValue)
        {
            TestFunctions commandType = (TestFunctions)commandValue;
            List<byte> commandBytes = new List<byte>();
            ConsoleHelper.WriteEmphasizedLine($"Sending Command: {commandType}", ConsoleColor.Yellow);
            switch (commandType)
            {
                case TestFunctions.SendSinglePacket:
                    ConsoleHelper.WriteEmphasizedLine("Enter payload message", ConsoleColor.Yellow);
                    string readMessage = Console.ReadLine();
                    byte[] convertedMessageBytes = Encoding.ASCII.GetBytes(readMessage);
                    SendPacket(destinationAddress, convertedMessageBytes);
                    return;
                case TestFunctions.SendMultipartPacket:
                    ConsoleHelper.WriteEmphasizedLine("Enter payload message", ConsoleColor.Yellow);
                    string readMultipartMessage = Console.ReadLine();
                    byte[] convertedMultipartMessageBytes = Encoding.ASCII.GetBytes(readMultipartMessage);
                    SendMultipartPacket(destinationAddress, convertedMultipartMessageBytes);
                    return;
                case TestFunctions.SendCountingTest:
                    byte[] countBytes = new byte[1024];
                    for (int i = 0; i < countBytes.Length; i++)
                    {
                        countBytes[i] = (byte)(i % 256);
                    }
                    SendMultipartPacket(destinationAddress, countBytes);
                    return;
                case TestFunctions.DisplayTest:
                    // Display Logo
                    byte[] displayCommand = new byte[] { (byte)SerialCommandType.DisplayControl, 1, (byte)DisplayType.RadioDogeLogo };
                    port.Write(displayCommand, 0, displayCommand.Length);
                    Thread.Sleep(2000);
                    // Display doge animation
                    displayCommand[2] = (byte)DisplayType.DogeAnimation;
                    port.Write(displayCommand, 0, displayCommand.Length);
                    Thread.Sleep(2000);
                    // Display coin animation
                    displayCommand[2] = (byte)DisplayType.CoinAnimation;
                    port.Write(displayCommand, 0, displayCommand.Length);
                    Thread.Sleep(2000);
                    // Display logo again
                    displayCommand[2] = (byte)DisplayType.RadioDogeLogo;
                    port.Write(displayCommand, 0, displayCommand.Length);
                    return;
                case TestFunctions.RunSPV:
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
                case TestFunctions.StopSPV:
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

        private void PrintTestCommandHelp()
        {
            ConsoleHelper.WriteEmphasizedLine("Available Test Mode Commands:", ConsoleColor.Magenta);
            foreach (int i in Enum.GetValues(typeof(TestFunctions)))
            {
                ConsoleHelper.WriteEmphasizedLine($"{i}: {(TestFunctions)i}", ConsoleColor.Cyan);
            }
            ConsoleHelper.WriteEmphasizedLine("Enter 'exit' to return to the mode selection screen or 'help' for available commands\n", ConsoleColor.Magenta);
        }
    }
}
