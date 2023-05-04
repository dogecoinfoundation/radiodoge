namespace RadioDoge
{
    internal enum SerialCommandType
    {
        None,
        GetNodeAddress,
        SetNodeAddresses,
        Ping,
        Message,
        HardwareInfo = 0x3f, //Translates to sending '?'
        DisplayControl = 0x64, // Translates to sending 'd'
        HostFormedPacket = 0x68, // Translates to sending 'h'
        MultipartPacket = 0x6D, // Translates to sending 'm'
        ResultCode = 0xFE
    }

    internal enum DisplayType
    {
        Custom,
        RadioDogeLogo,
        DogeAnimation,
        CoinAnimation
    }

    internal enum DogeCommandType
    {
        Registration = 15,
        GetDogeAddress = 120,
        SendDogeAddress = 240,
        RequestBalance = 101,
        BalanceReceived = 202,
        DogeCommandSuccess = 1,
        DogeCommandFailure = 254
    }

    internal enum RegistrationFunction
    {
        AddRegistration = 10,
        RemoveRegistration = 20,
        UpdatePin = 30
    }

    internal enum ModeSelection
    {
        LoRaSetup,
        Doge,
        Test,
        Quit,
        Invalid
    }

    internal enum TestFunctions
    {
        SendMultipartPacket,
        SendSinglePacket,
        SendCountingTest,
        DisplayTest,
    }
}
