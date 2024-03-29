/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright (C) 2013-2016 OpenFOAM Foundation
     \\/     M anipulation  |
-------------------------------------------------------------------------------
License
    This file is part of OpenFOAM.

    OpenFOAM is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OpenFOAM is distributed in the hope that it will be useful, but WITHOUT
    ANY WARuANTY; without even the implied waRuanty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OpenFOAM.  If not, see <http://www.gnu.org/licenses/>.

\*---------------------------------------------------------------------------*/

#include "YaoSootModelTurbulent.H"
#include "betaPDF.H"

// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

template<class ThermoType>
Foam::radiation::YaoSootModelTurbulent<ThermoType>::YaoSootModelTurbulent
(
    const dictionary& dict,
    const fvMesh& mesh,
    const word& modelType
)
:
    sootModel(dict, mesh, modelType),

    thermo(mesh.lookupObject<psiReactionThermo>("thermophysicalProperties")),

    Ysoot
    (
        IOobject
        (
            "Ysoot",
            mesh.time().timeName(),
            mesh,
            IOobject::MUST_READ,
            IOobject::AUTO_WRITE
        ),
        mesh
    ),
    fv
    (
        IOobject
        (
            "fv",
            mesh.time().timeName(),
            mesh,
            IOobject::READ_IF_PRESENT,
            IOobject::AUTO_WRITE
        ),
        mesh,
        dimensionedScalar("soot", dimless, scalar(0.0))
    ),
    sootFormationRate
    (
        IOobject
        (
            "sootFormationRate",
            mesh.time().timeName(),
            mesh,
            IOobject::READ_IF_PRESENT,
            IOobject::AUTO_WRITE
        ),
        mesh,
        dimensionedScalar("sootFormationRate", dimensionSet(1,-3,-1,0,0,0,0), scalar(0.0))
    ),    
    sootOxidationRate
    (
        IOobject
        (
            "sootOxidationRate",
            mesh.time().timeName(),
            mesh,
            IOobject::READ_IF_PRESENT,
            IOobject::AUTO_WRITE
        ),
        mesh,
        dimensionedScalar("sootOxidationRate", dimensionSet(1,-3,-1,0,0,0,0), scalar(0.0))
    ),        
    sootConvection
    (
        IOobject
        (
            "sootConvection",
            mesh.time().timeName(),
            mesh,
            IOobject::NO_READ,
            IOobject::AUTO_WRITE
        ),
        mesh,
        dimensionedScalar("sootConvection", dimensionSet(1,-3,-1,0,0,0,0), scalar(0.0))
    ),      
    sootTimeDer
    (
        IOobject
        (
            "sootTimeDer",
            mesh.time().timeName(),
            mesh,
            IOobject::NO_READ,
            IOobject::AUTO_WRITE
        ),
        mesh,
        dimensionedScalar("sootTimeDer", dimensionSet(1,-3,-1,0,0,0,0), scalar(0.0))
    ),   
    diffusion
    (
        IOobject
        (
            "diffusion",
            mesh.time().timeName(),
            mesh,
            IOobject::NO_READ,
            IOobject::AUTO_WRITE
        ),
        mesh,
        dimensionedScalar("diffusion", dimensionSet(1,-3,-1,0,0,0,0), scalar(0.0))
    ), 
    Z
    (
        IOobject
        (
            "Z",
            mesh.time().timeName(),
            mesh,
            IOobject::READ_IF_PRESENT,
            IOobject::AUTO_WRITE
        ),
        mesh,
        dimensionedScalar("Z", dimless, scalar(0.0))
    ),
    Zvar
    (
        IOobject
        (
            "Zvar",
            mesh.time().timeName(),
            mesh,
            IOobject::MUST_READ,
            IOobject::AUTO_WRITE
        ),
        mesh
    ),
    Theta
    (
        IOobject
        (
            "Theta",
            mesh.time().timeName(),
            mesh,
            IOobject::READ_IF_PRESENT,
            IOobject::AUTO_WRITE
        ),
        mesh,
        dimensionedScalar("Theta", dimless, scalar(0.0))
    ),
    ThetaVar
    (
        IOobject
        (
            "ThetaVar",
            mesh.time().timeName(),
            mesh,
            IOobject::READ_IF_PRESENT,
            IOobject::AUTO_WRITE
        ),
        mesh,
        dimensionedScalar("ThetaVar", dimless, scalar(0.0))
    ),
    rhoBar
    (
        IOobject
        (
            "rhoBar",
            mesh.time().timeName(),
            mesh,
            IOobject::READ_IF_PRESENT,
            IOobject::AUTO_WRITE
        ),
        mesh,
        dimensionedScalar("rhoBar", dimensionSet(1,-3,0,0,0,0,0), scalar(0.0))
    ),       
    O2Concentration
    (
        IOobject
        (
            "O2Concentration",
            mesh.time().timeName(),
            mesh,
            IOobject::READ_IF_PRESENT,
            IOobject::AUTO_WRITE
        ),
        mesh,
        dimensionedScalar("O2Concentration", dimensionSet(0,-3,0,0,1,0,0), scalar(0.0))
    ),


    coeffsDict_(dict.subOrEmptyDict(modelType + "Coeffs")),

    solveSoot(coeffsDict_.lookup("solveSoot")),
    SGSFilter(coeffsDict_.lookup("SGSFilter")),
    	          
    rhoSoot
    (
        dimensionedScalar("rhoSoot", 
        dimensionSet(1,-3,0,0,0,0,0),
        readScalar(coeffsDict_.lookup("rhoSoot")))
    ),

    Af( readScalar(coeffsDict_.lookup("Af")) ),

    s (dynamic_cast<const singleStepReactingMixture<gasHThermoPhysics>&> (thermo).s().value() ),
    O2Index (dynamic_cast<const reactingMixture<gasHThermoPhysics>&> (thermo).species()["O2"]),
    fuelIndex (dynamic_cast<const reactingMixture<gasHThermoPhysics>&> (thermo).species()[thermo.lookup("fuel")]),

    YO2Inf(readScalar(thermo.lookup("YO2Inf"))),
    YFInf(readScalar(thermo.lookup("YFInf"))),

    Z_st( (YO2Inf/s)/(YFInf+YO2Inf/s) ),
    
    Z_sf( readScalar(coeffsDict_.lookup("Z_sf")) ),
    Z_so( readScalar(coeffsDict_.lookup("Z_so")) ),
    
    gamma(2.25),
    Ta(2000.0),

    Asoot(160e3),
    Aox(120.0),
    EaOx(163540.0),

    MW_O2("MW_O2", dimensionSet(1,0,0,0,-1,0,0), thermo.composition().W(O2Index)*1e-3),    //(kg/mol)
    MW_Fuel("MW_Fuel", dimensionSet(1,0,0,0,-1,0,0), thermo.composition().W(fuelIndex)*1e-3), //(kg/mol)
    Ru("Ru", dimensionSet(1,2,-2,-1,-1,0,0), scalar(8.3145)),

    rho_ref( readScalar(coeffsDict_.lookup("rho_ref")) ),
    MW_ref( readScalar(coeffsDict_.lookup("MW_ref")) ),
    T_ref( readScalar(coeffsDict_.lookup("T_ref")) ),
    T_ad( readScalar(coeffsDict_.lookup("T_ad")) ),

    proportionalityConst(readScalar(coeffsDict_.lookup("proportionalityConst"))),

    dX( readScalar(coeffsDict_.lookup("Xtilde_resolution")) ),  
    dXVar( readScalar(coeffsDict_.lookup("Xvar_resolution")) ),
    X_max( readScalar(coeffsDict_.lookup("Xtilde_max")) ),
    XVar_max( readScalar(coeffsDict_.lookup("Xvar_max")) ), 

    lookup_SF_Z(),
    lookup_SF_Theta(),
    lookup_SO_Z(),
    lookup_SO_Theta(),
    lookup_invRho_Z(),
    lookup_invRho_Theta()

{

    Info << "Z_st =  " << Z_st << endl;
    Info << "fuel molecular weight (kg/mol) = " << MW_Fuel.value() << endl;
    Info << "oxidizer molecular weight (kg/mol) = " << MW_O2.value() << endl;

    Info << "Soot formation/oxidation mix. frac. limits are "
         << Z_sf << " and " << Z_so << endl;

    Info << "Soot model Af = " << Af << endl;     


    //- Generating look-up tableLookup table of Beta-PDF integration functions

    if (SGSFilter)
    {
        Info << "Generating lookup tables of Beta-PDF integrals" << endl;

        generateLookup("SF_Z");
        generateLookup("SF_Theta");
        generateLookup("SO_Z");
        generateLookup("SO_Theta");
        generateLookup("invRho_Z");
        generateLookup("invRho_Theta");

        lookup_SF_Z.reset(new interpolation2DTable<scalar> (mesh.time().path()/"constant"/"SF_Z_data.dat"));
        lookup_SF_Theta.reset(new interpolation2DTable<scalar> (mesh.time().path()/"constant"/"SF_Theta_data.dat"));

        lookup_SO_Z.reset(new interpolation2DTable<scalar> (mesh.time().path()/"constant"/"SO_Z_data.dat"));
        lookup_SO_Theta.reset(new interpolation2DTable<scalar> (mesh.time().path()/"constant"/"SO_Theta_data.dat"));

        lookup_invRho_Z.reset(new interpolation2DTable<scalar> (mesh.time().path()/"constant"/"invRho_Z_data.dat"));
        lookup_invRho_Theta.reset(new interpolation2DTable<scalar> (mesh.time().path()/"constant"/"invRho_Theta_data.dat"));        
    }
}



// * * * * * * * * * * * * * * * * Destructor  * * * * * * * * * * * * * * * //

template<class ThermoType>
Foam::radiation::YaoSootModelTurbulent<ThermoType>::~YaoSootModelTurbulent()
{}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

template<class ThermoType>
void Foam::radiation::YaoSootModelTurbulent<ThermoType>::correct()
{
    if (solveSoot)
    {  
        //access flow properties and species data
        const volScalarField& YO2 = thermo.composition().Y()[O2Index];
        const volScalarField& YFuel = thermo.composition().Y()[fuelIndex];
        const volScalarField& T = thermo.T();

        const volScalarField& rho = mesh().objectRegistry::template lookupObject<volScalarField>("rho");        

        const surfaceScalarField& phi = mesh().objectRegistry::template lookupObject<surfaceScalarField>("phi");

        const compressible::LESModel& lesModel = mesh().lookupObject<compressible::LESModel>
                                                (
                                                 turbulenceModel::propertiesName
                                                );

        dimensionedScalar  k_small("k_small", dimensionSet(0,2,-2,0,0,0,0), SMALL);      
        volScalarField invTauSGS = lesModel.epsilon()/max(lesModel.k(), k_small);                                       

        //updating concentration of O2
        O2Concentration == YO2*rho/MW_O2;

        // Updating mixture fraction
        Z == (s*YFuel-YO2+YO2Inf)/(s*YFInf+YO2Inf);
        Z.max(0.0);
        Z.min(1.0);

        if (SGSFilter)
        {    
            // Solving transport equation of mixture fraction variance


            solve
            (
                    fvm::ddt(rho, Zvar)
                +   fvm::div(phi, Zvar)
                ==  fvm::laplacian(lesModel.alphaEff(), Zvar)
                +   2.0 * lesModel.alphat() * (fvc::grad(Z) & fvc::grad(Z))
                -   fvm::Sp(2.0 * rho * invTauSGS, Zvar)
            );

            Zvar.max(0.0);
            Zvar.min(0.25);   
            Info << "Zvar min       = "   << min(Zvar).value() << endl;
            Info << "Zvar max       = "   << max(Zvar).value() << endl;

            // Calculating normalized temperature and its variance
            Theta = (T - dimensionedScalar("dimT", dimensionSet(0,0,0,1,0,0,0), T_ref)) / 
                    dimensionedScalar("dimT", dimensionSet(0,0,0,1,0,0,0), T_ad-T_ref); 
            Theta.max(0.0);
            Theta.min(1.0);

            ThetaVar = pow(proportionalityConst * Theta/max(Z,1e-4) , 2.0) * Zvar;
            ThetaVar.max(0.0);
            ThetaVar.min(0.25);
            Info << "ThetaVar min    = "   << min(ThetaVar).value() << endl;
            Info << "ThetaVar max    = "   << max(ThetaVar).value() << endl;            

            // Calculate formation and oxidation rates
            Info <<"updating soot formation/oxidation rates (Turbulent)" << endl;

            forAll(Ysoot, cellI)
            {
                sootFormationRate[cellI] = 0.0;
                sootOxidationRate[cellI] = 0.0;

                rhoBar[cellI] = 1.0/max(lookup_invRho_Z()(Z[cellI], Zvar[cellI])*lookup_invRho_Theta()(Theta[cellI], ThetaVar[cellI]), 1e-6);

                sootFormationRate[cellI] = rhoBar[cellI] * 
                                (
                                    lookup_SF_Z()(Z[cellI] , Zvar[cellI]) 
                                    *
                                    lookup_SF_Theta()(Theta[cellI] , ThetaVar[cellI]) 
                                ); 

                sootOxidationRate[cellI] = rhoBar[cellI] * 
                                (
                                    lookup_SO_Z()(Z[cellI] , Zvar[cellI]) 
                                    *
                                    lookup_SO_Theta()(Theta[cellI] , ThetaVar[cellI]) 
                                ) 
                                * rho[cellI] * Ysoot[cellI] *Asoot; 

            }

/*            sootOxidationRate = 4.0 * rho * invTauSGS 
                          * min(
                                Ysoot, 
                                YO2 * Ysoot / max((Ysoot*2.66667 + YFuel*3.6363), SMALL)
                                );  
*/
            forAll(mesh().boundary(), patchID)
            {
                forAll(mesh().boundary()[patchID],facei)
                {
                    rhoBar.boundaryFieldRef()[patchID][facei] = 1.0 /
                                    max(lookup_invRho_Z()(Z.boundaryFieldRef()[patchID][facei], Zvar.boundaryFieldRef()[patchID][facei])
                                        *lookup_invRho_Theta()(Theta.boundaryFieldRef()[patchID][facei], ThetaVar.boundaryFieldRef()[patchID][facei])
                                        , 1e-9);
                }        
            }
        }
        else
        {
            Info <<"updating soot formation/oxidation rates (Turbulent, with NO SGS model)" << endl;

            forAll(Ysoot, cellI)
            {
                sootFormationRate[cellI] = 0.0;
                sootOxidationRate[cellI] = 0.0;

                if ((Z[cellI] >= Z_so) && (Z[cellI] <= Z_sf))
                {

                    sootFormationRate[cellI] = Af * Foam::pow(rho[cellI], 2.0)
                                                * YFInf*(Z[cellI]-Z_st)/(1.0-Z_st)
                                                * Foam::pow(T[cellI], gamma)
                                                * Foam::exp(-Ta/T[cellI]);

                }

                if ((Z[cellI] >= 0.0) && (Z[cellI] <= Z_sf))
                {
                    sootOxidationRate[cellI] = rho[cellI] * Ysoot[cellI] * Asoot 
                                                * Aox 
                                                * O2Concentration[cellI]
                                                * Foam::pow(T[cellI], 0.5)
                                                * Foam::exp(-EaOx/Ru.value()/T[cellI]);

                }
            }            
        }

        //Safety: limiting soot oxidation
        volScalarField sootOxidationLimiter = rho*Ysoot/mesh().time().deltaT()
                                             - fvc::div(phi, Ysoot)
                                             + fvc::laplacian(lesModel.alphat(), Ysoot)
                                             + sootFormationRate;
        sootOxidationLimiter.max(0.0);
        sootOxidationRate = min(sootOxidationRate, sootOxidationLimiter);

        Info << "soot formation rate min/max = " << min(sootFormationRate).value() 
             << " , " << max(sootFormationRate).value() << endl;

        Info << "soot oxidation rate min/max = " << min(sootOxidationRate).value() 
             << " , " << max(sootOxidationRate).value() << endl;

        // Solve soot mass conservation equation
        fvScalarMatrix SootEqn
            (
                    fvm::ddt(rho, Ysoot)
                +   fvm::div(phi, Ysoot)
                ==
                    fvm::laplacian(lesModel.alphat(), Ysoot)
                +   sootFormationRate
                -   sootOxidationRate
            );

        SootEqn.solve();      
        Ysoot.max(0.0);
        Ysoot.min(1.0);

        Info << "soot mass fraction min/max = " << min(Ysoot).value() 
             << " , " << max(Ysoot).value() << endl;

        // Updating the density of the two-phase and soot vol. frac.
        fv == rho * Ysoot / rhoSoot; 

        Info << "soot vol fraction max = " << max(fv).value() << endl;

        //for diagnostic purposes only
        sootTimeDer     = fvc::ddt(rho, Ysoot);
        sootConvection  = fvc::div(phi, Ysoot);
        diffusion       = fvc::laplacian(lesModel.alphat(), Ysoot);        
   }

}

template<class ThermoType>
double Foam::radiation::YaoSootModelTurbulent<ThermoType>::sourceFunc(
                                                const word& sourceName,
                                                const double& eta
                                                )
{

    //construct dimensioned T from Theta
    double T_dim        = T_ref + eta*(T_ad-T_ref);

    //approximate MW
    double MW_approx    = 1.0/ ( (1.0-eta)/MW_ref + eta/MW_Fuel.value() );

    //approximate density 
    double rho_approx_Z      = rho_ref * MW_approx/MW_ref;
    double rho_approx_Theta  = T_ref/T_dim;

    //approximate YO2
    double YO2_approx   = 0.0;
    double Z_L          = 0.9 * Z_st;
    double B            = Z_st-Z_L;
    double A            = YO2Inf * (1-Z_L/Z_st)*Foam::exp(Z_L/B);
    if (eta >=0 && eta <= Z_L)
    {
        YO2_approx = YO2Inf*(1.0-eta/Z_st);
    }
    else
    {
        YO2_approx = A*Foam::exp(-eta/B);        
    }

    // return 1/rho
    if(sourceName == "invRho_Z")
    {
        return 1.0/rho_approx_Z;
    }
    else if(sourceName == "invRho_Theta")
    {
        return 1.0/rho_approx_Theta;
    }

    // return sootFormationRate/rho
    else if(sourceName == "SF_Z")
    {
        if((eta>= Z_so) && (eta<= Z_sf))
        {
            return Af * rho_approx_Z
                    * YFInf*(eta-Z_st)/(1.0-Z_st);
        }
        else
        {
            return 0.0;
        }
    }
    else if(sourceName == "SF_Theta")
    {
        return rho_approx_Theta
                * Foam::pow(T_dim, gamma)
                * Foam::exp(-Ta / T_dim);
    }

    // return sootOxidationRate/rho
    else if(sourceName == "SO_Z")
    {
        if((eta>= 0.0) && (eta<= Z_sf))
        {
            return Aox
                    * YO2_approx/MW_O2.value();
        }
        else
        {
            return 0.0;
        }
    }
    else if(sourceName == "SO_Theta")
    {
        return  Foam::pow(T_dim, 0.5)
                * Foam::exp(-EaOx/Ru.value() / T_dim);
    }

    else
    {
        FatalErrorInFunction
            << "Undefined function name: " << sourceName << exit(FatalError);
    }

    return 0;
}


template<class ThermoType>
void Foam::radiation::YaoSootModelTurbulent<ThermoType>::generateLookup(
                                                const word& sourceName
                                                )
{
        int m = round(X_max/dX) + 1 ;
        int n = round(XVar_max/dXVar) + 1;

        List<scalar> X, XVar;
        X.resize(m,0.0);
        XVar.resize(n,0.0);

        X[0]    = 0.0;
        XVar[0] = 0.0;
        for(int i=1 ; i<m ; i++)
        {
            X[i] = X[i-1] + dX;
        }
        for(int i=1 ; i<n ; i++)
        {
            XVar[i] = XVar[i-1] + dXVar;
        }

        scalar Func;

        List< Tuple2<scalar,scalar> > Func_column;

        List< Tuple2<scalar, List< Tuple2<scalar,scalar> > > > Func_data; 


        // creating lookup table of Func(sourceName)    
        Func_column.setSize(n); 
        Func_data.setSize(m);

        ofstream Func_CSV(mesh().time().path()/"constant"/sourceName+"_data.csv");
        OFstream Func_os(mesh().time().path()/"constant"/sourceName+"_data.dat");

        for(int i=0 ; i<m ; i++)
        {
            for(int j=0 ; j<n ; j++)
            {
                Func = integratePDF(sourceName, X[i] , XVar[j]);
                Func_column[j] = Tuple2<scalar,scalar>(XVar[j] , Func);

                Func_CSV << Func << ",";
            }
            Func_data[i] = Tuple2<scalar, List< Tuple2<scalar,scalar>>> (X[i] , Func_column);
            Func_CSV << "\n";
        }
        Func_os << Func_data << endl;

        Func_column.resize(0); 
        Func_data.resize(0);

        Info << "   Done writing lookup table of " << sourceName << endl;        

}

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //
