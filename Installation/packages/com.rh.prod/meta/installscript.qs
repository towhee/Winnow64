/****************************************************************************
**
COMPONENT = prod: All dll and resources unlikely to change
**
****************************************************************************/

function Component()
{
    // constructor
}

Component.prototype.isDefault = function()
{
    // select the component by default
    return true;
}

Component.prototype.createOperations = function()
{
    try {
        // call the base create operations function
        /*
        if (installer.value("os") === "win") {
            component.addOperation("Rmdir", "@TargetDir@");
        } */

        component.createOperations();

        if (installer.value("os") === "win") {
            component.addOperation("CreateShortcut", "@TargetDir@/winnow.exe", "@StartMenuDir@/Winnow.lnk");
        }
    } catch (e) {
    console.log(e);
    }
}
