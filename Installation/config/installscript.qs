function Controller()
{
}

Controller.prototype.IntroductionPageCallback = function()
{
    // Automatically press finish
    gui.clickButton(buttons.NextButton);
}

Controller.prototype.ComponentSelectionPageCallback = function()
{
// Automatically press finish
gui.clickButton(buttons.NextButton);
}

Controller.prototype.FinishedPageCallback = function()
{
// Automatically press finish
gui.clickButton(buttons.FinishButton);
}
