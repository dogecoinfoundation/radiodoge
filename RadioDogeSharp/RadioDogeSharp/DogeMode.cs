using System.Text;

namespace RadioDoge
{
    public partial class SerDogeSharp
    {
        private Dictionary<string, AddressBookEntry> dogeAddressBook = new Dictionary<string, AddressBookEntry>();
        private const int PIN_LENGTH = 4;
        private const int MAX_ADDRESS_LENGTH = 35;
        private byte[] testAddress = { 0x44 };

        private void SendDogeCommand(int commandValue)
        {
            DogeCommandType dogeCommandType = (DogeCommandType)commandValue;
            List<byte> commandBytes = new List<byte>();
            ConsoleHelper.WriteEmphasizedLine($"Sending Command: {dogeCommandType}", ConsoleColor.Yellow);
            switch (dogeCommandType)
            {
                case DogeCommandType.GetDogeAddress:
                    RequestDogeCoinAddress(destinationAddress);
                    return;
                case DogeCommandType.RequestBalance:
                    RequestDogeCoinBalance(destinationAddress, testAddress);
                    break;
                default:
                    ConsoleHelper.WriteEmphasizedLine("Unknown command", ConsoleColor.Red);
                    break;
            }
            byte[] commandToSend = commandBytes.ToArray();
            PrintCommandBytes(commandToSend);
            port.Write(commandToSend, 0, commandToSend.Length);
        }

        private void RequestDogeCoinAddress(NodeAddress destNode)
        {
            // Create payload
            byte[] payload = new byte[] { (byte)DogeCommandType.GetDogeAddress };
            SendPacket(destNode, payload);
        }

        private void RequestDogeCoinBalance(NodeAddress destNode, byte[] dogeCoinAddress)
        {
            List<byte> payload = new List<byte>(dogeCoinAddress.Length + 1);
            payload.Add((byte)DogeCommandType.RequestBalance);
            payload.AddRange(dogeCoinAddress);
            SendPacket(destNode, payload.ToArray());
        }

        private void SendDogeCoinBalance(NodeAddress destNode, byte[] serializedBalance)
        {
            List<byte> payload = new List<byte>(serializedBalance.Length + 1);
            payload.Add((byte)DogeCommandType.BalanceReceived);
            payload.AddRange(serializedBalance);
            SendPacket(destNode, payload.ToArray());
        }

        private void ProcessDogePayload(NodeAddress senderAddress, byte[] payload)
        {
            DogeCommandType commType = (DogeCommandType)payload[0];
            switch (commType)
            {
                case DogeCommandType.SendDogeAddress:
                    // This means we got sent a dogecoin address
                    int addrLength = payload.Length - 1;
                    if (addrLength <= MAX_ADDRESS_LENGTH)
                    {
                        string addressString = ExtractDogecoinAddressFromPayload(1, addrLength, payload);
                        Console.WriteLine($"Received Dogecoin Address: {addressString}");
                    }
                    else
                    {
                        Console.WriteLine("Received an address that was too long!");
                    }
                    break;
                case DogeCommandType.GetDogeAddress:
                    // This means someone is requesting a dogecoin address so we would send ours out
                    SendDogeAddress(senderAddress);
                    break;
                case DogeCommandType.RequestBalance:
                    ServiceBalanceRequest(senderAddress, payload);
                    break;
                case DogeCommandType.Registration:
                    RegisterDogeAddress(senderAddress, payload);
                    break;
                default:
                    Console.WriteLine("Unknown payload. Raw data:");
                    PrintPayloadAsHex(payload);
                    break;
            }
        }

        private void ServiceBalanceRequest(NodeAddress replyAddress, byte[] payload)
        {
            int addressLength = payload.Length - 1;
            string requestAddress = ExtractDogecoinAddressFromPayload(1, addressLength, payload);
            if (dogeAddressBook.ContainsKey(requestAddress))
            {
                // Get the balance
                // @TODO
                float testBalance = 320.2032f;

                // Modify it with the pin
                byte[] pin = dogeAddressBook[requestAddress].GetPin();
                byte[] balanceToSend = ObfuscateDogeCoinBalance(testBalance, pin);

                // Printing for debug purposes REMOVE LATER
                float obfuscatedValue = BitConverter.ToSingle(balanceToSend, 0);
                Console.WriteLine($"Balance: {testBalance}, Obfuscated: {obfuscatedValue}");

                // Send out obfuscated balance
                SendDogeCoinBalance(replyAddress, balanceToSend);
            }
            else
            {
                // Address book does not contain requested address.
                // wat do?
            }
        }

        private string ExtractDogecoinAddressFromPayload(int offsetIndex, int addrLength, byte[] payload)
        {
            char[] dogecoinAddress = new char[addrLength];
            Array.Copy(payload, offsetIndex, dogecoinAddress, 0, addrLength);
            return new string(dogecoinAddress);
        }

        private void RegisterDogeAddress(NodeAddress sender, byte[] payload)
        {
            // First get the registration function to be performed
            RegistrationFunction regOpcode = (RegistrationFunction)payload[1];
            int addressLength = payload.Length;
            if (regOpcode == RegistrationFunction.UpdatePin)
            {
                addressLength -= (2 * PIN_LENGTH) + 2;
            }
            else
            {
                addressLength -= PIN_LENGTH + 2;
            }

            if (addressLength <= MAX_ADDRESS_LENGTH)
            {
                string addressString = ExtractDogecoinAddressFromPayload(2, addressLength, payload);
                // Extract pin from payload
                byte[] extractedPin = new byte[PIN_LENGTH];
                Array.Copy(payload, addressLength + 2, extractedPin, 0, PIN_LENGTH);
                switch (regOpcode)
                {
                    case RegistrationFunction.UpdatePin:
                        if (dogeAddressBook.ContainsKey(addressString))
                        {
                            // Check if old pin matches new pin
                            byte[] updatedPin = new byte[PIN_LENGTH];
                            Array.Copy(payload, addressLength + 2 + PIN_LENGTH, updatedPin, 0, PIN_LENGTH);
                            bool pinUpdateSuccess = dogeAddressBook[addressString].ResetPin(extractedPin, updatedPin);
                            if (pinUpdateSuccess)
                            {
                                Console.WriteLine($"Pin successfully updated for {addressString}");
                            }
                            else
                            {
                                Console.WriteLine($"Pin could not be updated for {addressString}");
                            }
                        }
                        else
                        {
                            Console.WriteLine("Address has not been previously registered!");
                        }
                        break;
                    case RegistrationFunction.AddRegistration:
                        bool alreadyRegistered = dogeAddressBook.ContainsKey(addressString);
                        if (!alreadyRegistered)
                        {
                            Console.WriteLine($"Registering Dogecoin Address: {addressString}");
                            string pinString = GetStringFromPin(extractedPin);
                            Console.WriteLine($"Pin: {pinString}");
                            AddressBookEntry entry = new AddressBookEntry(sender, extractedPin);
                            dogeAddressBook.Add(addressString, entry);
                        }
                        else
                        {
                            Console.WriteLine("Address already registered!");
                        }
                        break;
                    case RegistrationFunction.RemoveRegistration:
                        if (dogeAddressBook.ContainsKey(addressString))
                        {
                            // Check if pin matches
                            if (dogeAddressBook[addressString].DoesPinMatch(extractedPin))
                            {
                                dogeAddressBook.Remove(addressString);
                                Console.WriteLine($"Address {addressString} successfully removed!");
                            }
                            else
                            {
                                Console.WriteLine($"Pin does not match for {addressString}. Unable to remove registration!");
                            }
                        }
                        break;
                }
            }
            else
            {
                Console.WriteLine($"Failed to perform registration function: {regOpcode}");
            }

            // Print address book for debugging purposes
            Console.WriteLine();
            PrintDogeAddressBook();
        }

        private byte[] ObfuscateDogeCoinBalance(float balance, byte[] pin)
        {
            byte[] serializedBalance = BitConverter.GetBytes(balance);
            for (int i = 0; i < serializedBalance.Length; i++)
            {
                serializedBalance[i] ^= pin[i];
            }
            return serializedBalance;
        }

        private float DeobfuscateDogeCoinBalance(byte[] serializedBalance, byte[] pin)
        {
            for (int i = 0; i < serializedBalance.Length; i++)
            {
                serializedBalance[i] ^= pin[i];
            }
            return BitConverter.ToSingle(serializedBalance, 0);
        }

        private string GetStringFromPin(byte[] pin)
        {
            string pinString = "";
            for (int i = 0; i < PIN_LENGTH; i++)
            {
                pinString += pin[i].ToString();
            }
            return pinString;
        }

        private void SendDogeAddress(NodeAddress destNode)
        {
            // Generate a test address for now
            string privatekey;
            string publickey;

            int len = 256;
            StringBuilder pvkey = new StringBuilder(len);
            StringBuilder pubkey = new StringBuilder(len);
            LibDogecoin.dogecoin_ecc_start();
            int successret = LibDogecoin.generatePrivPubKeypair(pvkey, pubkey, 0);

            if (successret == 1)
            {
                privatekey = pvkey.ToString();
                publickey = pubkey.ToString();
                Console.WriteLine($"Public key: {publickey}");

                byte[] dogeCoinAddress = Encoding.ASCII.GetBytes(publickey);
                List<byte> payload = new List<byte>(dogeCoinAddress.Length + 1);
                payload.Add((byte)DogeCommandType.SendDogeAddress);
                payload.AddRange(dogeCoinAddress);
                SendPacket(destNode, payload.ToArray());
            }

            LibDogecoin.dogecoin_ecc_stop();
        }

        private void PrintDogeCommandHelp()
        {
            ConsoleHelper.WriteEmphasizedLine("Available Doge Mode Commands:", ConsoleColor.Magenta);
            foreach (int i in Enum.GetValues(typeof(DogeCommandType)))
            {
                ConsoleHelper.WriteEmphasizedLine($"{i}: {(DogeCommandType)i}", ConsoleColor.Cyan);
            }
            ConsoleHelper.WriteEmphasizedLine("Enter 'exit' to return to the mode selection screen or 'help' for available commands\n", ConsoleColor.Magenta);
        }

        private void PrintDogeAddressBook()
        {
            Console.WriteLine("### Dogecoin Address Book ###");
            Dictionary<string, AddressBookEntry>.KeyCollection keys = dogeAddressBook.Keys;
            foreach (string dogeAddress in keys)
            {
                Console.WriteLine($"Address: {dogeAddress}, Node: {dogeAddressBook[dogeAddress].GetNodeAddress()}");
            }
        }
    }
}
