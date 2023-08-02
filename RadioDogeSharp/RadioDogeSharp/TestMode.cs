using System.Text;

namespace RadioDoge
{
    public partial class SerDogeSharp
    {
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
                    portManager.SendPacket(localAddress, destinationAddress, convertedMessageBytes);
                    return;
                case TestFunctions.SendMultipartPacket:
                    ConsoleHelper.WriteEmphasizedLine("Enter payload message", ConsoleColor.Yellow);
                    string readMultipartMessage = Console.ReadLine();
                    byte[] convertedMultipartMessageBytes = Encoding.ASCII.GetBytes(readMultipartMessage);
                    portManager.SendMultipartPacket(localAddress, destinationAddress, convertedMultipartMessageBytes);
                    return;
                case TestFunctions.SendCountingTest:
                    byte[] countBytes = new byte[1024];
                    for (int i = 0; i < countBytes.Length; i++)
                    {
                        countBytes[i] = (byte)(i % 256);
                    }
                    portManager.SendMultipartPacket(localAddress, destinationAddress, countBytes);
                    return;
                case TestFunctions.SendBroadcastTest:
                    SendBroadcast();
                    return;
                case TestFunctions.DisplayTest:
                    // Display custom string
                    DisplayCustomStringOnOLED("TEST CUSTOM STRING!!!\nSECOND LINE\nTHIRD LINE\nFOURTH LINE\nFIFTH LINE", 0);
                    Thread.Sleep(4000);
                    // Display Logo
                    byte[] displayCommand = new byte[] { (byte)SerialCommandType.DisplayControl, 1, (byte)DisplayType.RadioDogeLogo };
                    portManager.WriteToPort(displayCommand, 0, displayCommand.Length);
                    Thread.Sleep(4000);
                    // Display doge animation
                    displayCommand[2] = (byte)DisplayType.DogeAnimation;
                    portManager.WriteToPort(displayCommand, 0, displayCommand.Length);
                    Thread.Sleep(4000);
                    // Display coin animation
                    displayCommand[2] = (byte)DisplayType.CoinAnimation;
                    portManager.WriteToPort(displayCommand, 0, displayCommand.Length);
                    Thread.Sleep(4000);
                    // Display logo again
                    displayCommand[2] = (byte)DisplayType.RadioDogeLogo;
                    portManager.WriteToPort(displayCommand, 0, displayCommand.Length);
                    return;
                case TestFunctions.LibDogecoinTest:
                    LibdogecoinFunctionalityTesting();
                    break;
                default:
                    ConsoleHelper.WriteEmphasizedLine("Unknown command", ConsoleColor.Red);
                    break;
            }
        }

        private void DisplayCustomStringOnOLED(string customDisplayMessage, int yOffset)
        {
            List<byte> commandPayload = new List<byte>();
            int payloadLength = 2 + customDisplayMessage.Length;
            byte[] commandHeader = new byte[] { (byte)SerialCommandType.DisplayControl, (byte)payloadLength, (byte)DisplayType.StringDisplay, (byte) yOffset };
            commandPayload.AddRange(commandHeader);
            byte[] convertedBytes = Encoding.ASCII.GetBytes(customDisplayMessage);
            commandPayload.AddRange(convertedBytes);
            byte[] commandBytes = commandPayload.ToArray();
            portManager.WriteToPort(commandBytes, 0, commandBytes.Length);
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
