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
                    SendPacket(destinationAddress, convertedMessageBytes);
                    return;
                case TestFunctions.SendMultipartPacket:
                    ConsoleHelper.WriteEmphasizedLine("Enter payload message", ConsoleColor.Yellow);
                    string readMultipartMessage = Console.ReadLine();
                    byte[] convertedMultipartMessageBytes = Encoding.ASCII.GetBytes(readMultipartMessage);
                    SendMultipartPacket(convertedMultipartMessageBytes);
                    return;
                case TestFunctions.SendCountingTest:
                    byte[] countBytes = new byte[1024];
                    for (int i = 0; i < countBytes.Length; i++)
                    {
                        countBytes[i] = (byte)(i % 256);
                    }
                    SendMultipartPacket(countBytes);
                    return;
                case TestFunctions.DisplayTest:
                    byte[] displayCommand = new byte[] { (byte)SerialCommandType.DisplayControl, 1, (byte)DisplayType.RadioDogeLogo };
                    port.Write(displayCommand, 0, displayCommand.Length);
                    Thread.Sleep(2000);
                    displayCommand = new byte[] { (byte)SerialCommandType.DisplayControl, 1, (byte)DisplayType.DogeAnimation };
                    port.Write(displayCommand, 0, displayCommand.Length);
                    Thread.Sleep(2000);
                    displayCommand = new byte[] { (byte)SerialCommandType.DisplayControl, 1, (byte)DisplayType.CoinAnimation };
                    port.Write(displayCommand, 0, displayCommand.Length);
                    Thread.Sleep(2000);
                    displayCommand = new byte[] { (byte)SerialCommandType.DisplayControl, 1, (byte)DisplayType.RadioDogeLogo };
                    port.Write(displayCommand, 0, displayCommand.Length);
                    return;
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
